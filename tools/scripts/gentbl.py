from decimal import Decimal
from sys import argv

# Constants for bits
_MSB = 1 << 63
_U64MAX = (1 << 64) - 1

# Returns the normalized 64-bit mantissa of a number and the power of it in 2^x
def normalize(x):
    if x == 0:
        # x is already 0
        return (x, 0)
    elif x <= _U64MAX:
        p = 63

        # Move to the 'left'
        while x & _MSB == 0:
            x = x << 1
            p = p - 1 # Decrease power

        if implicit_one:
            x = x << 1
            x = x & _U64MAX

        return (x, p)
    else:
        p = 63

        # Move to the 'right'
        while x > _U64MAX:
            x = x >> 1
            p = p + 1 # Increase power

        if implicit_one:
            x = x << 1
            x = x & _U64MAX

        return (x, p)

# Returns the 64-bit mantissa of 10^exp10
def gen_mantissa(exp10):
    # Generate positive exponent mantissas
    if exp10 >= 0:
        # Generate significand
        x = 10 ** exp10

        # Return normalized mantissa
        return normalize(x)

    # Generate negative exponent mantissa
    x = 0                               # Non-normalized mantissa
    f = Decimal(10) ** exp10            # We use this to calculate digits
    first_one = 0                       # What is the index of the first 1
    bit_idx = 0                         # What bit are we currently on?
    found_one = False                   # Have we found the first 1 bit?

    # Use slow method to find mantissa
    while f != 1:
        f = f * 2
        x = x << 1
        if f > 1:
            x = x | 1
            f = f % 1
            if not found_one:
                first_one = bit_idx
                found_one = True

        if found_one and bit_idx - first_one > 128:
            break
        bit_idx = bit_idx + 1

    # Normalize x
    return normalize(x)

# Get uppercase hex
def hexupper(x):
    return '0x' + hex(x).upper()[2:]

# Generate mantissa table
def gen_mantissa_table(start, end, step):
    t = []
    for e in range(start, end, step):
        t.append(gen_mantissa(e))
    return t

# Print C table
def ctable(prefix, start, end, step=1):
    t = gen_mantissa_table(start, end, step)

    (i, e10) = (0, start)
    while i + 2 <= len(t):
        (m_0, e2_0) = t[i]
        (m_1, e2_1) = t[i+1]
        print(prefix, end='')
        print(hexupper(m_0), end=', ')
        print(hexupper(m_1), end=',')

        if print_detail:
            print(f'\t// 1e{e10}', end='')
            e10 = e10 + step
            print(f', 1e{e10}')
            e10 = e10 + step
        else:
            print()
            e10 = e10 + step * 2

        if print_powers:
            print(f'// 2^{e2_0}, 2^{e2_1}')
                
        i = i + 2

    if i != len(t):
        (m, e2) = t[i]
        print(prefix, end='')
        print(hexupper(m), end=',')

        if print_detail:
            print(f'\t\t\t// 1e{e10}')
        else:
            print()

        if print_powers:
            print(f'// 2^{e2}')

# Get command line arguments
print_detail = False # Flag of whether or not to print comments
implicit_one = False # Whether or not to assume implicit one
print_powers = False # Print the power values in 2^x

for arg in argv:
    print_detail = print_detail or arg == '-d' or arg == '--detail'
    implicit_one = implicit_one or arg == '-i' or arg == '--implicit-one'
    print_powers = print_powers or arg == '-p' or arg == '--powers'

print('// Don\'t touch this file, this is auto generated by gentbl.py')
print('// Include this in function to have local variables')
print('// Also include stdint.h before this')
print(f'#ifndef _tables_h_')
print(f'#define _tables_h_\n')
print(f'//#include <stdint.h>\n')

print('// Fine table')
print('static const uint64_t fine[] = {')
ctable('\t', 0, 28)
print('};\n')

print('\n// Coarse table')
print('static const uint64_t coarse[] = {')
ctable('\t', -330, 310, 28)
print('};\n')

print(f'#endif // _tables_h_')
