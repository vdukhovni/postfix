#include <stdio.h>
#include <ctype.h>

void    skip_comment()
{
    int     c;

    while ((c = getchar()) != EOF) {
	if (c == '*') {
	    if ((c = getchar()) == '/') {
		return;
	    }
	    if (c != EOF)
		ungetc(c, stdin);
	}
    }
}

void    copy_string(int quote)
{
    int     c;

    while ((c = getchar()) != EOF) {
	if (c == quote) {
	    return;
	} else if (c == '\\') {
	    if ((c = getchar()) == EOF) {
		return;
	    } else if (isalpha(c)) {
		putchar(' ');
	    } else {
		putchar('\\');
		putchar(c);
	    }
	} else {
	    putchar(c);
	}
    }
}

int     main()
{
    int     c;

    while ((c = getchar()) != EOF) {
	switch (c) {
	case '/':
	    if ((c = getchar()) == '*') {
		skip_comment();
	    } else if (c == '/') {
		while ((c = getchar()) != EOF) {
		    if (c == '\n')
			break;
		}
	    } else {
		if (c != EOF)
		    ungetc(c, stdin);
	    }
	    break;
	case '"':
	case '\'':
	    copy_string(c);
	    putchar('\n');
	    break;
	case '\\':
	    (void) getchar();
	    break;
	default:
	    break;
	}
    }
}
