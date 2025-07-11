#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		const char* format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 'd') {
			format++;
			int num = va_arg(parameters, int);
			char buffer[11]; // Max integer value is 2147483647 (10 digits), + 1 for '-'
			char* ptr;
			char* low;
			char* str;
			str = ptr = buffer;
			// Add '-' for negative numbers
			if (num < 0)
				*ptr++ = '-';
			low = ptr;
			do {
				*ptr++ = "0123456789"[num % 10];
				num /= 10; 
			} while (num);
			*ptr-- = '\0';
			// Reverse
			while (low < ptr) {
				char temp = *low;
				*low++ = *ptr;
				*ptr-- = temp;
			}
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 'x') {
			format++;
			unsigned int num = va_arg(parameters, unsigned int);
			char buffer[8];
			char* ptr;
			char* low;
			char* str;
			str = low = ptr = buffer;
			do {
				*ptr++ = "0123456789abcdef"[num % 16];
				num /= 16; 
			} while (num);

			*ptr-- = '\0';
			// Reverse
			while (low < ptr) {
				char temp = *low;
				*low++ = *ptr;
				*ptr-- = temp;
			}
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else {
			format = format_begun_at;
			size_t len = strlen(format);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, len))
				return -1;
			written += len;
			format += len;
		}
	}

	va_end(parameters);
	return written;
}
