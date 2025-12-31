# Pecco Language Prelude
# This file is automatically loaded by the compiler
# Contains all built-in operators and core functions

# ===== Core Functions =====

# Basic I/O - wraps libc write
func write(fd: i32, buf: string, count: i32) : i32;

# ===== Arithmetic Operators (Binary) =====

# Addition
infix operator + (a: i32, b: i32) : i32 prec 70 assoc left;
infix operator + (a: f64, b: f64) : f64 prec 70 assoc left;

# Subtraction
infix operator - (a: i32, b: i32) : i32 prec 70 assoc left;
infix operator - (a: f64, b: f64) : f64 prec 70 assoc left;

# Multiplication
infix operator * (a: i32, b: i32) : i32 prec 80 assoc left;
infix operator * (a: f64, b: f64) : f64 prec 80 assoc left;

# Division
infix operator / (a: i32, b: i32) : i32 prec 80 assoc left;
infix operator / (a: f64, b: f64) : f64 prec 80 assoc left;

# Modulo
infix operator % (a: i32, b: i32) : i32 prec 80 assoc left;

# Power
infix operator ** (a: i32, b: i32) : i32 prec 90 assoc right;
infix operator ** (a: f64, b: f64) : f64 prec 90 assoc right;

# ===== Unary Operators =====

# Negation
prefix operator - (a: i32) : i32;
prefix operator - (a: f64) : f64;

# Logical NOT
prefix operator ! (a: bool) : bool;

# ===== Bitwise Operators =====

# Bitwise AND
infix operator & (a: i32, b: i32) : i32 prec 50 assoc left;

# Bitwise OR
infix operator | (a: i32, b: i32) : i32 prec 40 assoc left;

# Bitwise XOR
infix operator ^ (a: i32, b: i32) : i32 prec 45 assoc left;

# Left Shift
infix operator << (a: i32, b: i32) : i32 prec 65 assoc left;

# Right Shift
infix operator >> (a: i32, b: i32) : i32 prec 65 assoc left;

# ===== Logical Operators =====

# Logical AND
infix operator && (a: bool, b: bool) : bool prec 30 assoc left;

# Logical OR
infix operator || (a: bool, b: bool) : bool prec 20 assoc left;

# ===== Comparison Operators =====

# Equality
infix operator == (a: i32, b: i32) : bool prec 55 assoc none;
infix operator == (a: f64, b: f64) : bool prec 55 assoc none;
infix operator == (a: bool, b: bool) : bool prec 55 assoc none;
infix operator == (a: string, b: string) : bool prec 55 assoc none;

# Inequality
infix operator != (a: i32, b: i32) : bool prec 55 assoc none;
infix operator != (a: f64, b: f64) : bool prec 55 assoc none;
infix operator != (a: bool, b: bool) : bool prec 55 assoc none;
infix operator != (a: string, b: string) : bool prec 55 assoc none;

# Less than
infix operator < (a: i32, b: i32) : bool prec 60 assoc none;
infix operator < (a: f64, b: f64) : bool prec 60 assoc none;

# Greater than
infix operator > (a: i32, b: i32) : bool prec 60 assoc none;
infix operator > (a: f64, b: f64) : bool prec 60 assoc none;

# Less than or equal
infix operator <= (a: i32, b: i32) : bool prec 60 assoc none;
infix operator <= (a: f64, b: f64) : bool prec 60 assoc none;

# Greater than or equal
infix operator >= (a: i32, b: i32) : bool prec 60 assoc none;
infix operator >= (a: f64, b: f64) : bool prec 60 assoc none;
