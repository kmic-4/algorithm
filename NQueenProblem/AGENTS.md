# AGENTS.md

## Purpose

This repository implements the **N-Queens Problem in C using an Island Model Genetic Algorithm (GA)**.

This file defines instructions for coding agents working in the repository. It is **not** the algorithm specification. The authoritative source for algorithm design, mathematical definitions, supported strategies, and experiment requirements is:

- [`NQueens_IslandGA_Implementation_Spec.md`](./NQueens_IslandGA_Implementation_Spec.md)

**Read the specification before implementing or modifying algorithm-related code.** Do not recreate or independently reinterpret its algorithm descriptions in this file.

## Source of Truth and Decision Rules

1. Follow the user's latest explicit instructions and the algorithm specification.
2. Within the specification, distinguish **[確定]** (confirmed), **[暫定]** (provisional), and **[要検証]** (requires verification).
3. Implement confirmed requirements as written. Treat provisional choices as configurable defaults, not permanent restrictions.
4. If a requirement is ambiguous or contradicts another, identify the conflicting sections. Do not silently change the algorithm, fitness definition, selection semantics, or experimental conditions.
5. When algorithm behavior is intentionally changed, update the specification and associated tests in the same change. Keep `AGENTS.md` focused on agent workflow and engineering policy.
6. The referenced GraphColoringProblem repository is an architectural example, **not** a source of N-Queens algorithm requirements or unquestioned implementation details.

## Implementation Principles

- **Language:** C. Prefer portable, standard C and explicitly document any platform-specific dependencies.
- **Configuration:** Centralize tunable values in `definition.h` or the configuration type specified by the implementation. Avoid scattering numeric constants or strategy switches across modules.
- **Modularity:** Isolate parent selection, crossover, mutation, fitness evaluation, migration topology, migrant selection, and replacement behind type-safe interfaces and function pointers where appropriate.
- **Separation of concerns:** Keep the GA control loop independent of the concrete operator implementations. Place module declarations in headers and implementations in corresponding `.c` files.
- **Ownership:** Specify who allocates, owns, copies, and frees each buffer. Never confuse shallow structure copies with deep copies of dynamically allocated arrays.
- **Error handling:** Validate configuration and allocation results. Return explicit errors or terminate cleanly; do not continue with invalid state.
- **Consistency:** Keep cached fitness and conflict data synchronized with the genes. Never use stale evaluation data.
- **Reproducibility:** Record random seeds and effective settings. Avoid sharing unsafe global random state across islands or threads.
- **Dependencies:** Keep the build lightweight. Do not introduce external libraries without a clear justification.

## Performance Requirements

Performance is a design objective, but **correctness comes first**.

- Preserve the asymptotic complexity targets stated in the specification.
- Avoid redundant fitness evaluations, whole-population copies, and allocations inside hot loops when buffers can be reused.
- Favor contiguous storage and predictable memory access; use pointers to eliminate unnecessary copies, not to introduce avoidable indirection.
- Reuse workspace buffers where safe and keep workspace state isolated between concurrent workers.
- Cache derived information only when its cost, lifetime, and invalidation rules are clear.
- Do not claim a speedup without measurement. Compare elapsed time, fitness evaluations, memory use, and solution quality under comparable settings.
- Do not change the defined GA behavior purely for micro-optimization.

## Development Workflow

For each implementation task:

1. Read the relevant sections of `NQueens_IslandGA_Implementation_Spec.md`.
2. Identify the affected modules, public interfaces, parameters, and invariants.
3. Make the smallest coherent implementation change, keeping alternate strategies interchangeable.
4. Add or update focused tests, especially where genetic operators change chromosome content or cached evaluation data.
5. Compile with warnings enabled and run relevant correctness tests.
6. Check memory safety and deterministic replay where tooling is available.
7. Report the files changed, tests run, results, and any deviations or unresolved decisions.

If an essential algorithm detail is marked provisional, retain configurability and document the chosen default. Do not add an entirely new strategy merely because it might improve performance.

## Validation and Testing

Use the specification's testing section as the detailed checklist. At a minimum, verify:

- Gene values remain within the permitted bounds after every operator.
- Fitness and per-queen conflict information remain mutually consistent.
- An independent reference evaluator agrees with the optimized fitness evaluator on small and randomized cases.
- Elite preservation and migration copies do not unintentionally modify source individuals.
- Island-to-island transfers respect the selected configuration and do not alias mutable buffers.
- Deterministic runs reproduce results for the same configuration and seed, under the stated execution assumptions.
- Memory management has no leaks, invalid accesses, or double frees.

Recommended compiler checks where supported:

```bash
cc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -g ...
```

For debug and memory-safety builds, enable AddressSanitizer and UndefinedBehaviorSanitizer when the toolchain supports them. Do not assume a build system or test command exists until you inspect the repository.

## Documentation and Experiment Integrity

- Keep algorithm descriptions and formal requirements in `NQueens_IslandGA_Implementation_Spec.md`.
- Keep this file limited to engineering instructions and references.
- Update README/build instructions if compilation or execution commands change.
- Log enough configuration and result data to reproduce experiments.
- When comparing strategies, change only the intended variable(s), or explicitly identify confounding changes.
- Clearly separate verified measurements from hypotheses or performance expectations.

## Completion Criteria

A task is complete when the implementation follows the applicable specification, builds successfully, its relevant tests pass, and any remaining provisional decisions or limitations are explicitly reported.
