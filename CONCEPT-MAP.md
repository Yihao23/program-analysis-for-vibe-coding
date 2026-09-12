<!-- CONCEPT-MAP.md -->
# Concept → code (in lecture order)

This file is the correspondence table between `program-analysis-for-vibe-coding` and *Program Analysis* lectures 01–09. The README keeps only how to run and the scenarios; the concept mapping lives here.

Four columns per row: **the lecture concept** · **what the lecture says** · **mapping onto the gate** (what each symbol from the lecture is in the gate) · **code location**.

```
★  directly implemented   there is a piece of code in the gate doing exactly this
≈  analogy                structurally corresponding, but not the same mathematical object
—  design rationale       shaped how things were written; no corresponding code
```

Line numbers refer to the current versions of `gate/gate.cpp` and `specs/specs.cpp`.

## 01 Introduction

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ Rice's theorem | non-trivial semantic properties are undecidable | There is no check called "is the code correct". Only concrete properties, one per `SPEC` | `specs.cpp:50,59,97` |
| ★ `Init ⊆ I`, `Next(I) ⊆ I`, `I ∩ Bad = ∅` | inductive invariant: start inside I, one step stays inside I, I contains no bad state | `Init` = the current `app/`; `Next` = one change by the agent; `I` = the conjunction of all checks; `Bad` = any ✗. `gate check` = verifying `Next(I) ⊆ I` | `gate.cpp:276` |
| ★ static vs dynamic | over-approximation misses no bug but may raise false alarms; under-approximation raises no false alarm but may miss bugs | UPPER section = over-approximating checks (assertions over all inputs); LOWER section = under-approximating checks (concrete executions). Complementary, not interchangeable | `gate.cpp:288-297` |

## 02 Data-Flow Analysis

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ abstract domain D | the kinds of information the analysis can see | the five constants = the dimensions the gate can see: directory trust level, include relations, line counts, escape-hatch strings, tidy rules. Anything outside these dimensions is invisible to the gate | `gate.cpp:29-53` |
| ★ I must be an element of D | an invariant must be expressible in the domain | the convention "features do not touch billing" exists only once written as a `Contract` struct; an unwritten one does not exist for an AI | `gate.cpp:41` |
| ★ I spanning several components | an invariant involving several variables | a rule involving several files: written in the INVARIANT block of the umbrella header, checked by a cross-file SPEC | `users.hpp:2-5` ↔ `specs.cpp:50` |
| ≈ monotone `a ⊑ b ⟹ f(a) ⊑ f(b)` | more precise input, more precise output | more precise context for the agent, more predictable output; also the precondition for the change→check→change loop to converge | CLAUDE.md |
| ≈ Kleene `lfp = fⁿ(⊥)` | iterate from ⊥ until nothing changes | agent changes → gate → changes → … until VERDICT "mergeable". This loop is the Kleene iteration | CLAUDE.md protocol step 3 |
| ★ `⊥` | most precise / empty state | `fresh()`: every SPEC first clears all global state | `specs.cpp:40` |
| ≈ ACC | ascending chains must stabilise | the check system must have a floor: constraints cannot be loosened indefinitely — one of the reasons escape hatches are banned | `gate.cpp:49` |

## 03a Worklist

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ `Dep_b` | the set of locations depending on b | reverse `#include` graph: who includes b | `gate.cpp:316-322` |
| ★ closure `W ≔ W ∪ Dep_b` | recompute only the affected locations | `affected` computes the impact closure of a change (`.cpp` first maps to the `.hpp` of the same name). `check` does not currently use it to prune; it conservatively runs everything | `gate.cpp:325-331` |
| ≈ bounded in/out-degree → `O(h·|V|)` | sparse dependencies make increments cheap | `K_MAX_INCLUDES` bounds the includes per file, keeping the impact radius controllable | `gate.cpp:46` |

## 03b Widening / Narrowing

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| — widening axiom `d₁ ⊔ d₂ ⊑ d₁ ∇ d₂` | jump upward, never lose anything, sound | the agent rapidly producing a generalised solution = widening: usable, but overshoots | — |
| ★ threshold set K | jump to constants found in the program instead of negotiating step by step | functions ≤ 40 lines, includes ≤ 6: the constraints given to the agent are thresholds, not "it depends" | `gate.cpp:46` → `check_K` (187) |
| ≈ narrowing is anytime | top-down, every step sound, can stop at any time | run reduce after generating to remove redundancy and simplify; stopping after one step is still sound, no need to finish in one go | CLAUDE.md "when done" |

## 04 Abstract Interpretation I

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| — concrete semantics `f(Δ)` | one step on a set of real states | the effect of the agent's change on the program's **real behaviour**. Not computable, but the anchor of every soundness argument | — |
| ≈ Galois `l ⊑ γ(α(l))` | abstract then concretise: coarser than the original | what the gate knows about the code = the dimensions in the declaration area, always coarser than the code. "Understanding AI code" = being precise enough on the dimensions you care about, not reading every line | `gate.cpp:29-53` |
| ★ α vs spec | α is computed from the code; the spec is not in the code | `app/` has only α (what it does); `specs/` is the spec (what it should do). Checking = α(code) satisfies spec | `dedupe.hpp:2-3` ↔ `specs.cpp` |
| ★ upper approximation `f(γ(m)) ⊑ γ(f#(m))` | one abstract step must cover one concrete step | `α` = `check()` computing a report from the code; `m` = the baseline report; `γ(m)` = **every codebase that would produce the same report** (everything the gate cannot distinguish; no code, described by the blind-spot list); `f` = the change applied to the real code; `f#` = running `check()` again after the change. **The scope of the check must cover the whole impact of the change**: file-local properties look at `scope`, cross-file properties scan everything, behavioural checks run in full | `gate.cpp:277-286`; see the section below |
| ★ fixpoint transfer `lfp(f) ⊑ γ(lfp(f#))` | locally sound at every step ⟹ globally sound | `f` = one PR; `fⁱ(⊥)` = the real codebase after the i-th PR; `lfp(f#)` = the gate's accumulated conclusion. Every PR passing the gate individually ⟹ the accumulated "green" holds for the whole codebase, no global review needed. Precondition: every step f has a matching f#, i.e. **no PR is merged without going through the gate** (enforced by merge policy, not implemented in the demo). Note: the current `check()` recomputes everything, reports do not carry across PRs, so this theorem has no teeth yet; it becomes a hard precondition once `affected()` pruning or trust labels are relied on | `gate.cpp:276` |
| — best `f# = α∘f∘γ` | the most precise upper approximation; γ is infinite so it cannot be implemented | ideal review = unfold the whole impact of a change and compress it back into the check system. The gate is a coarse approximation of it | — |

## 05 Abstract Interpretation II

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ reduction operator `ρ = α∘γ` | compress several representations of the same concrete value to the smallest | `fingerprint`: parameter names → `_argN`, whitespace removed; two functions with the same fingerprint = redundant representations of the same behaviour | `gate.cpp:201, 216` |
| ★ without reduction even straight-line code loses precision | the mathematical form of rot | `dedupe_dup`: functionally correct, just one representation too many. Run reduce after every task, not at the quarterly refactor | `candidates/dedupe_dup.hpp` |
| ≈ independent attribute method loses relations | abstract per component, drop constraints between components | per-file review = independent attribute method: cross-file relations are lost | — |
| ★ relational method | abstract jointly, keep constraints | the cross-file SPEC (the composite result of write → read) | `specs.cpp:50` |
| ≈ sequential composition | an abstraction of an abstraction is still an abstraction | types → include contracts → trust zones, layer by layer; reviewing the upper layer does not require looking at the lower one | — |

## 06 Logic / SMT

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ `F valid ⟺ ¬F unsatisfiable` | solvers only look for counterexamples | every check looks for a counterexample; "none found" is all the guarantee you get | `specs.cpp:97-111` |
| ≈ trade expressiveness for decidability | decidable only without multiplication, without quantifiers | every constraint is restricted (line counts, strings, include prefixes), which is why it can be decided mechanically. "The code must be elegant" is undecidable, so it is not in the list | `gate.cpp:29-53` |
| ≈ theory = signature + axioms | reason within the axioms | each `CONTRACT` is an axiom; whether the agent's output is valid under the axioms is decidable | `gate.cpp:41` |

## 07 Symbolic Abstraction

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ best local steps compose into a bad global result | each of the three swap steps is best, the whole explodes | two sessions each "best" (session 1 adds a cache, session 2 rewrites the write), the composition breaks. Check the composite result, not the single step | `specs.cpp:50`, scenario 5 |
| ≈ γ̂ symbolic concretisation | represent an infinite set by a formula | one `SPEC(upper, …)` = a property "over all inputs" expressed in code | `specs.cpp:97` |
| ★ RSY: lower bound only | the solver returns some model, not the one you want | given only examples, the agent produces the minimal program satisfying them (`std::set`) | scenario 1 |
| ★ counterexample S and β(S) | generalise the counterexample and join it into the lower bound | random search for a counterexample, `shrink` to minimise; the agent treats it as an input the next version must handle | `specs.cpp:98-108` |
| ★ Bilateral: raise lower, tighten upper | approach from both sides, stop when they meet | `lower` = LOWER section (confirmed must-be-able-to); `upper` = UPPER section (confirmed must-not-cross); `mid` = the agent's proposal; solver = the gate; convergence = "upper ✓ lower ✓" | `gate.cpp:288-297` |
| ★ `l ⊑ AC(l,u) ∧ u ⋢ AC(l,u)` | every mid must make strict progress on one side | `l ⊑ mid`: no regression → LOWER reruns in full; `u ⋢ mid`: the proposal must not be ⊤ in disguise → escape hatches banned (NOLINT = a proposal that promises nothing); "progress whichever way the answer goes" → every ✗ carries `detail` (= S) | `gate.cpp:279, 296`, `Result.detail`; see the section below |
| ★ timeout returns a non-trivial upper bound | where Bilateral beats RSY | the human not having time to review = timeout. The mechanical upper bound still holds → VERDICT "upper ✓ lower ✗ → wait for review", instead of knowing nothing | `gate.cpp:305-310` |
| ★ the generator optimises against the check | the solver gives the laziest model | escape hatches = the shortest path to a green check; banned outright for AI | `gate.cpp:49, 177` |
| ★ false alarms became cheap | the precision/cost optimum moves | regeneration is nearly free → turn on checks you would not dare turn on for humans: full `-Werror`, tidy warnings as errors | `CMakeLists.txt:8`, `gate.cpp:53` |
| ≈ analyse a loop-free fragment as a whole | symbolic execution of the whole fragment keeps the relations | review at the granularity of intent, not of the diff: the cross-file SPEC | `specs.cpp:50` |

## 09 Information Flow

| Concept | In the lecture | Mapping onto the gate | Location |
|---|---|---|---|
| ★ integrity lattice | information must not flow from untrusted to trusted | `TRUSTED = app/core` (human-reviewed), `UNTRUSTED = app/features` (AI output, not reviewed). The report labels the tier | `gate.cpp:33-34` |
| ★ provenance becomes a security level | — | unreviewed AI output = untrusted. The level describes "has someone vouched for it", not quality | CLAUDE.md trust zones |
| ★ explicit flow | via data flow / dependencies | the `#include` contract | `gate.cpp:41, 167`, scenario 3 |
| ★ implicit flow / context G | via control rather than assignment | a forward declaration changing the global `settings()`: no edge in the include graph, yet billing's behaviour changes | `billing.hpp:4` ↔ `promo_extern.cpp`, scenario 4 |
| ★ non-interference `M₁ =_L M₂ ⟹ P(M₁) =_L P(M₂)` | two initial states equal on low variables end in states equal on low variables | `M₁` = the world without features linked, `M₂` = the world with; `=_L` = same trusted part; `P(·)` = the ledger. Both ledgers must agree | `CMakeLists.txt:13,24-25` + `golden.cpp` + `gate.cpp:258` |
| ★ hyperproperty | a property of **sets** of traces, invisible on a single trace | HYPER is its own section: cannot rely on a single test, two worlds must be built | `gate.cpp:297` |
| ★ OBJECT library | — | a static library drops unreferenced .o files together with their static initialisers; OBJECT guarantees the implicit flow can be observed | `CMakeLists.txt:12-13` |

Not used: lecture 08 (symbolic / concolic execution). It is a technique for finding triggering inputs and could automatically extend the test set of agent-written code; not done in this demo.

## The three load-bearing ones

```
Next(I) ⊆ I                     gate.cpp:276   check()
f(γ(m)) ⊑ γ(f#(m))              gate.cpp:277   scope + affected()
l ⊑ AC(l,u) ∧ u ⋢ AC(l,u)       gate.cpp:288   the upper / lower sections
```

## What `f(γ(m)) ⊑ γ(f#(m))` means inside the gate

First match every symbol:

```
α           check(): computes a report from the code                        gate.cpp:276
m           the baseline report (all ✓)
γ(m)        every codebase that would produce this report — everything the gate cannot distinguish
            no code; the "known blind spots" section is its description
f           the agent's change applied to the real code
f(γ(m))     apply this change to every program in γ(m); all possible outcomes
f#          run check() again after the change
γ(f#(m))    every codebase that would produce the new report
```

A green report does not say "this program is correct"; it says "this program belongs to γ(m), and every program in that set passed these checks".
γ can never be computed (lectures 04/07), but it can be described: it is everything the gate does not look at. Every added check shrinks γ(m) by one ring — the size of γ is the precision of the gate.

The inequality says: one abstract step must cover every possibility of one concrete step.
In the gate, "the concrete step" is the effect of the agent's change on the program's real behaviour, "the abstract step" is the checks the gate runs.
The inequality holds ⟺ **the scope of the checks covers the whole impact of the change**. In code this shows up as three scope decisions:

```
gate.cpp:277      scope = the changed files        arch / escape / K look only at these — these properties are file-local, the impact of a change stays inside the file
gate.cpp:281      reduce always scans all of app/  — duplication is a cross-file property; looking only at changed files would miss a duplicate elsewhere
gate.cpp:283-285  specs / hyper always run in full — behavioural impact can reach anywhere; cover it conservatively
```

`affected()` computes "the smallest scope that still covers the whole impact". `check()` does not currently use it to prune; it conservatively runs everything.
Pruning with it would require the dependency graph to **over-approximate** the real impact — and the `#include` graph does not (the forward declaration of scenario 4 is not in the graph), so HYPER must never be pruned by `affected()`. This is the shape the inequality takes in real engineering:
**every decision "we can run a little less this time" is a bet that f# still covers f.**

## What `l ⊑ AC(l,u) ∧ u ⋢ AC(l,u)` means inside the gate

The Bilateral algorithm of lecture 07 picks a `mid` each round, requiring `l ⊑ mid` (it covers the confirmed lower bound) and `u ⋢ mid` (it is strictly narrower than the upper bound).
Together they guarantee: whether the solver answers "counterexample" or "none", either lower or upper makes strict progress; the loop never idles.

Mapped onto the gate, `mid` is the agent's proposal, the solver is the gate, the loop is "proposal → report → fix → …":

```
l ⊑ mid          the proposal must not regress              gate.cpp:296   the LOWER section reruns in full every time, never pruned by the change
u ⋢ mid          the proposal must not be ⊤ in disguise     gate.cpp:279   escape hatches banned: a proposal with NOLINT promises nothing,
                                                                           the gate would forever say "no counterexample", upper makes zero progress
one side must    every ✗ must carry a counterexample       Result.detail  of() only FAILs when violations are non-empty; specs failures give FAIL lines;
progress                                                                   hyper failures give both ledgers. The gate never says "no, but I won't tell you why"
```

Honest note: in the lecture `mid` is computed by the algorithm; here it is proposed by the agent. The gate can detect regression (a ✗ in LOWER) and block known disguises (the escape-hatch list),
but it cannot stop the agent from proposing a regression, nor block unknown disguises. Half of the progress guarantee is in the gate (every answer carries a counterexample), half in the agent (fixing with the counterexample instead of switching disguise).
CLAUDE.md's "fix the code when it fails, do not work around it" is what supplies the agent's half.
