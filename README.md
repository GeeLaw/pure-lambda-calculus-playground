# Pure lambda calculus playground

A playground project accompanying my recreational reading about lambda calculus.

## Parser for [de Bruijn indexed](https://en.wikipedia.org/wiki/De_Bruijn_index) lambda terms

Lexical matters:

- A `lambda` token is `.` or `lambda`.
- A `const` token is a string matching `[A-Za-z~!$%^&*+=|\\/<>?_-][0-9A-Za-z~!$%^&*+=|\\/<>?_-]*`. Strange characters are allowed so that you might use `/` or `*` as identifiers of constants.
- A `var` token is a string matching `[0-9]+` and is between 1 and 65536 (inclusive).
- A `(` [resp. `)`] token is `(` [resp. `)`].
- An `invalid` token is generated if the lexer sees something that cannot be parsed as a token.
- White spaces are omitted, except perhaps for splitting tokens.

Grammar (CFG part):

- A lambda term is a `Term` (start symbol).
- `Term` goes to `ApplicationTermList AbstractionTerm`.
- `Term` goes to `ApplicationTermList ApplicationTerm`.
- `AbstractionTerm` goes to `lambda Term`.
- `ApplicationTermList` goes to empty string.
- `ApplicationTermList` goes to `ApplicationTermList ApplicationTerm`.
- `ApplicationTerm` goes to `const`.
- `ApplicationTerm` goes to `var`.
- `ApplicationTerm` goes to `( Term )`.

Extra grammar constraints:

- The face value of any `var` terminal must not exceed the number of `AbstractionTerm` ancestors it has.

Semantics (in the sense how such a string represents a lambda term in the usual writing system):

- Each `const` token represents a lambda term previously bound to an identifier. Say the `const` token contains identifier `SomeConst`, which is the lambda term `SomeTerm`. Then `SomeConst` can be replaced with `(SomeTerm)`.
- Each `var` token represents a bound variable. Let the face value of such a token be `n`, then the token represents the variable bound to the `n`-th most nested abstraction. For example, `λx.λy.xy` is `lambda lambda 2 1`, or simply `..2 1`.
- Unbound variables are not supported. As a workaround, you could add outer abstractions to bind all variables.
- Abstraction goes as far as possible.
- Application is left-associated.

## Rewriters for pure lambda terms

In the file `code/reducer.hpp` are the rewriters (and friends). It implements eta-conversion and beta-reduction (in normal order, with call-by-need a.k.a. memoised lazy evaluation).

The structures defined in the file follows visitor pattern. The visitor `LambdaCalculus::Reduction::EtaConversion` walks through the syntax tree, discovers oppotunities of eta-conversion and performs the rewriting. The visitor `DeepCloneAndReplace` is a helper to beta-reduction. It is used to do the substitution. Finally, there is `BetaReduction`, which performs one beta-reduction at a time in normal order, changing all the references to the reduced term (memoised evaluation).

The toy program `code/toys/parse-reduce-print.cpp` reads lambda terms, reduces them step by step, printing the intermediate results.

## Playground

There is a playground program located at `code/playground.cpp`. It can be used as an interactive console, or can be used as an interpreter.

The syntax is as the following:

- Each line consists of a command.
- If the line is `set<space><identifier><space><expression>`, the `<identifier>` is set to `<expression>`.
  - Note that though the program allows you to set an identifier more than once, setting it the second time will **NOT** affect the terms created before, as the substitution of terms is immediate.
- If the line is `reduce<space><identifier>`, the `<identifer>` is reduced and stored in-place. At most 65536 beta-reductions will be done.
- If the line is `print<space><identifier>`, the `<identifier>` is printed, followed by a new line character.
- If the line is `echo<space>.<anything>`, the `<anything>` is textually printed, followed by a new line character.
- If the line is `exit`, the program terminates.

You can `playground < tests.txt` to see it perform some basic lambda calculus.
