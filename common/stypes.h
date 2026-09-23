#ifndef _JLOS_COMMON_STYPE_H
#define _JLOS_COMMON_STYPE_H

static inline int jlos_islower(int c)
{
    return c >= 'a' && c <= 'z';
}

static inline int jlos_isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static inline int jlos_isalpha(int c)
{
    return jlos_islower(c) || jlos_isupper(c);
}

static inline int jlos_isdigit(int c)
{
    return c >= '0' && c <= '9';
}

static inline int jlos_isalnum(int c)
{
    return jlos_isalpha(c) || jlos_isdigit(c);
}

static inline int jlos_tolower(int c)
{
    return jlos_isupper(c) ? c + ('a' - 'A') : c;
}

static inline int jlos_toupper(int c)
{
    return jlos_islower(c) ? c - ('a' - 'A') : c;
}

#endif
