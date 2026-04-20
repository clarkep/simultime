#include <autil.h>
#include <stdio.h>

i32 replace_digit_in_number(i32 x, i32 digit_i, i32 digit_value)
{
	i32 pow10 = 1;
	for (i32 i=0; i<digit_i; i++) pow10 *= 10;
	i32 pow10plus = pow10 * 10;

	i32 d = (x % pow10plus) - (x % pow10);

	return x - d + digit_value*pow10;
}

int main(int argc, char *argv[])
{
	i32 x = 76543210;
	i32 y = replace_digit_in_number(x, 5, 9);
	printf("%d", y);
	return 0;
}