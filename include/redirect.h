/*
 * File: redirect.h
 * Description:
 * Header file for redirection support.
 * Declares the setup_redirection function used to
 * handle input, output, append, and error redirection.
 */

#ifndef REDIRECT_H
#define REDIRECT_H

int setup_redirection(char **args);

#endif