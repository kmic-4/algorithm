# 目的
nクイーン問題(N-Queens Problem)を解くこと。


# N-Queens Problem

## 1. Overview

The **N-Queens Problem** is a combinatorial problem that involves placing $N$ queens on an $N \times N$ chessboard such that no two queens can attack each other.

A queen can attack other pieces along the same row, column, or diagonal.

## 2. Mathematical Formulation

Let the position of queen $i$ be represented by $(r_i, c_i)$, where:

- $r_i$: Row index of queen $i$
- $c_i$: Column index of queen $i$

For any two distinct queens $i$ and $j$, the following constraints must be satisfied:

| Constraint | Mathematical Expression |
|---|---|
| Different rows | $r_i \neq r_j$ |
| Different columns | $c_i \neq c_j$ |
| Different diagonals | $\lvert r_i-r_j \rvert \neq \lvert c_i-c_j \rvert$ |

### Array Representation

By placing exactly one queen in each row, the row constraint is automatically satisfied.

A configuration can then be represented as an array of length $N$:

$$
X = [x_0, x_1, \ldots, x_{N-1}]
$$

where $x_i$ represents the column index of the queen in row $i$.

Under this representation, only two constraints remain:

$$
\begin{aligned}
x_i &\neq x_j \\
\lvert x_i-x_j \rvert &\neq \lvert i-j \rvert
\end{aligned}
\qquad (i \neq j)
$$

These constraints ensure that no two queens share the same column or diagonal.