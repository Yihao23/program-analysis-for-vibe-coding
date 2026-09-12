# Keeping AI-Generated Code Under Control, Seen Through Program Analysis

> Inspired by the *Program Analysis* lecture notes 01–09 (Jan Reineke, Saarland University):
> data-flow analysis · abstract interpretation · widening/narrowing · SMT · symbolic abstraction · symbolic/concolic execution · information-flow analysis

*中文版：[ai-code-under-control.zh.md](ai-code-under-control.zh.md)*

---

## The core claim

Nothing in the lectures is specific to AI — they are about *programs*, whoever wrote them.
But AI-generated code changes a few key parameters, and that turns concepts that already existed from "edge cases" into "the default case".

```
   Point 1:  AI makes every piece of code legacy code from the moment it is born
             ⟹ the spec must be externalised, and comparing α(code) against the spec must be done by a machine, not by the author
   Point 7:  AI makes false alarms from strict analysis almost free
             ⟹ the machine can finally afford to do that comparison

   Both happen at the same time.
   For the first time in fifty years, program analysis has "necessary" and "feasible" together.
```

The remaining five points (2–6) are this claim unfolded into a process.

---

## Background: the thinking framework the lectures give you

### Rice's theorem (lecture 01): "correct" is not a checkable property

```
  ✗  "Is this code correct?"                       ← undecidable, and not even definable
  ✓  "Can y be negative at the print statement?"   ← a concrete property, can be approximated
  ✓  "Did this module's public interface change?"
```

The first step is not "set up tests"; it is to write down the things you care about as a list of checkable properties.

### Inductive invariants (lectures 01/02): don't check every step, check what every step preserves

```
   Init ⊆ I              the initial state is inside I
   Next(I) ⊆ I           one step from I stays inside I
   I ∩ Bad = ∅           I contains no bad state

   Init = the current codebase     Next = one change by the AI
   I    = the healthy state        Bad  = the states you cannot accept
```

You do not control a fast-changing system by looking faster than it moves; you control it by making every step pass through the same gate.

### Static vs dynamic (lectures 01/08): squeeze from both sides

```
                                    no missed bug   no false alarm   always terminates
   static analysis (over-approx.)        ✓               ✗                ✓
   testing (under-approx.)               ✗               ✓                ✓
   perfect decision                      ✓               ✓                ✗   ← Rice says it does not exist

   lower side (under-approx.): tests, examples, property tests       → "it can at least do these"
   upper side (over-approx.):  types, lint, architectural constraints → "it will never do those"
```

### Transfer functions and "best" (lectures 02/04/07)

A transfer function `f_a : D → D` is the effect of one statement on an abstract value. The best transfer function `f# = α ∘ f ∘ γ`
is the most precise one for a given abstract domain. But **"best" is relative to the domain** — in the swap example every step is best,
yet the whole blows up from `[1,5]` to `[−104,204]`, because the interval domain cannot hold the relation `x′ = x − y`.

---

## Seven differences: AI code vs human-written code

Each point follows the pattern "structural difference → concrete scenario → lecture concept → what to do".

---

### 1. α(code) is a by-product of writing; AI code has no such by-product

**Structural difference**

Program analysis as a discipline was born for "code with no author to ask": legacy systems, binaries, third-party libraries.
When a human writes code, the act of writing produces an abstraction in the author's head (invariants, boundaries, design reasons) — a free by-product.
For AI-generated code that by-product lands in nobody's head. You can ask the AI, but that is another sample,
with no causal link to the intent at the time of writing.

**AI code = mass-produced code that is legacy code from birth.**

**Scenario**: an HTTP request function with retries

```python
def fetch(url, retries=5):
    for i in range(retries):
        r = http.get(url)
        if r.status < 500:
            return r
        sleep(2 ** i + random())
    raise RetryExhausted(url)
```

Six months later someone asks: "Does it retry on 429 (rate limited)?"

- **Human-written**: the author answers in two seconds: "No, 429 is the caller's own problem, I deliberately retry only on 5xx."
  That decision was never written down, but it lives in the author's head.
- **AI-written**: `r.status < 500` returns immediately on 429. Decision or coincidence? Nobody knows.
  If `< 500` was actually a typo (meant `< 400`), the AI's after-the-fact explanation will rationalise the typo.

**Lecture concept**: a correctness check = does `α(code)` satisfy the spec. α is mechanically computable ("what it actually does"),
and can be computed for AI code just as well; what is missing is the spec ("what it should do") — which in principle is not in the code, it exists only in the author's head or in the prompt.
For human code, the spec and the belief that "α(code) matches the spec" are by-products of writing; for AI code neither lands in anyone's head.

**What to do**: require the agent to produce a checkable spec alongside the code, not a natural-language explanation

```python
def test_no_retry_on_429():
    http.get = fake(status=429)
    fetch("u")
    assert http.get.call_count == 1
```

This test is the externalised spec. Running it is what computes α(code) and compares it with the spec. It turns "no retry on 429" from a coincidence into a property.

---

### 2. Every invocation is literally a transfer function, with no side channel

**Structural difference**

A human writing the third line of swap remembers what the first line did — `x′ = x − y` is in their head, not in the code's abstract state.
Humans have a side channel: memory, conversation, "here is what I was thinking".
An AI agent has no side channel. The input of each invocation is exactly the code plus the context window; the output is new code; nothing else in between.
Structurally this is `f#(abstract state) = new abstract state`.

For human code, "best local steps compose into a bad global result" is an occasional risk; for AI code it is the default mode of operation.

**Scenario**: three sessions, the same `users` module

```
Session 1  "add a cache to get_user"
           → the agent adds _cache: dict[int, User] in users/read.py

Session 2  "implement update_user"
           → the agent opens users/write.py, writes to the DB, tests pass
           → never opened read.py, does not know there is a cache. Reads after update return stale data.

Session 3  "fix the occasional stale data"
           → the agent adds a 60-second TTL to the cache
           → the system is now "eventually consistent" — by accident
```

Each step's input is the code it saw; each step's output is a reasonable result for that input.
Session 1's output had to be compressed into the representation "code", and there was nowhere to store the relation "every write must invalidate the cache".

**Lecture concept**: lecture 07, the composition of best transfer functions ≠ the best transfer function of the composition; a relation lost in the representation can never be recovered.

**What to do**: put the relation into the abstract state the next agent is guaranteed to see, and make it machine-checkable

```python
# users/__init__.py
# INVARIANT: every write to users must call read.invalidate(user_id)

def test_write_then_read_is_fresh():
    create_user(id=1, name="a")
    get_user(1)                      # fill the cache
    update_user(1, name="b")
    assert get_user(1).name == "b"   # stale reads are not allowed
```

The test spans both read and write files — it is what lecture 07 calls "analysing the whole fragment", checking the composite result.

---

### 3. Invariants must be externalised, because there is no persistent mind to hold them

**Structural difference**

A human working on a codebase for months keeps invariants in their head, many never written down.
`Next(I) ⊆ I` is executed implicitly for humans. An AI starts every session from ⊥;
any invariant that is not written down does not exist for it.

Not because the AI is dumber than a human, but because tacit agreement needs a continuously existing mind to carry it.

**Scenario**: a team convention — "never call the payment API directly from a request handler; always enqueue an async job".
Nobody has violated it in three years, and nobody has written it down. The AI receives the task "add a refund endpoint":

```python
@app.post("/refund")
def refund(order_id):
    payment_client.refund(order_id)     # called directly
    return {"ok": True}
```

All tests green, clean code, the reviewer may not even notice — what it violates is a rule that was never written down.

**Lecture concept**: lecture 02, the I in `Next(I) ⊆ I` must be an element of the domain the analyser can read, not a tacit agreement.

**What to do**: turn the tacit agreement into a machine-checked architectural constraint

```ini
# .importlinter
[importlinter:contract:no-sync-payment]
name = handlers must not touch payment client
type = forbidden
source_modules = app.handlers
forbidden_modules = app.payment_client
```

A line in CLAUDE.md helps too, but that is a hint; import-linter is enforcement.

---

### 4. The generator is a process that optimises against your checks

**Structural difference**

Humans write tests to help themselves and generally do not deliberately special-case around their own tests.
An AI agent facing the goal "make the tests green" can find the minimal program satisfying the lower bound.
Giving only a lower bound (tests) is using the AI as RSY: it will give you the laziest satisfying solution.
The upper bound must carry far more weight for AI code than for human code.

**Scenario**: you wrote a lower bound

```python
def test_dedupe():
    assert dedupe([1, 2, 2, 3]) == [1, 2, 3]
```

The AI produces:

```python
def dedupe(xs):
    return list(set(xs))
```

It passes — a set of small integers happens to preserve order. It is the minimal program satisfying this lower bound.
"Preserve first-occurrence order" was never said out loud. Switch to string inputs and the order scrambles immediately, but the test does not cover that.

Other shapes of the same pattern: a type error → add `# type: ignore`; a flaky test → add `@pytest.mark.skip`.

**Lecture concept**: lecture 07, RSY has only a lower bound, the solver returns *some* model rather than the one you want;
Bilateral converges only with an upper bound added.

```
   upper = ⊤  ──┐   constraints: types, lint, architecture rules, "must never …"
                │   ↓ tighten
                │   ↑ raise
   lower = ⊥  ──┘   examples: tests, use cases, "must be able to …"
   stop when they meet
```

**What to do**: add an upper bound — properties rather than samples

```python
from hypothesis import given, strategies as st

@given(st.lists(st.text()))
def test_dedupe_preserves_first_occurrence(xs):
    out = dedupe(xs)
    assert out == [x for i, x in enumerate(xs) if x not in xs[:i]]
```

Plus one lint rule: `warn_unused_ignores = true` and forbid any new `type: ignore`.

---

### 5. Provenance becomes a security level

**Structural difference**

A human-written codebase usually has a single trust level, backed by review.
AI naturally introduces a second level: unreviewed AI-generated code = untrusted, human-reviewed code = trusted.
And the untrusted layer grows faster than review can keep up.

Lecture 09's integrity: information must not flow from untrusted to trusted.
Non-interference gives something formalisable:

```
   ∀ two versions V₁, V₂ :
       V₁ and V₂ agree on the human-reviewed part
       ⟹  V₁ and V₂ agree on the critical observable behaviour
```

**Scenario**: a codebase with two layers

```
   trusted    core/billing/, core/auth/     human-reviewed
   untrusted  features/*                    rapidly AI-generated
```

To guarantee: "no change under features/ may alter billing amounts, except through the single entry point `billing.api.charge(amount)`."

A subtle violation — not a data flow, an implicit flow:

```python
# features/promo_banner.py
settings.rounding_mode = "floor"     # to make the price on the banner look nice
```

No line passes data into billing, but billing reads the global `settings`, and from now on every invoice rounds down.
This is exactly the shape of lecture 09's `if (h<4) l:=42 else l:=7` — through control/configuration dependency, not assignment.

**Lecture concept**: lecture 09, integrity = information must not flow from untrusted to trusted; the context G captures implicit flows.

**What to do**: two checks, one for explicit flow and one for implicit flow

```ini
# explicit flow: import rules
[importlinter:contract:billing-integrity]
type = forbidden
source_modules = features
forbidden_modules = core.billing.internal, core.settings   # note: settings is forbidden too
```

```python
# implicit flow: differential test (a direct implementation of non-interference)
def test_features_do_not_interfere_with_billing():
    ledger_a = run_billing_golden_suite(features_enabled=False)
    ledger_b = run_billing_golden_suite(features_enabled=True)
    assert ledger_a == ledger_b
```

The second test is the literal translation of `V₁ =_L V₂ ⟹ P(V₁) =_L P(V₂)`.

---

### 6. The human is the decision procedure that times out

**Structural difference**

For human code, "nobody reviewed it" still leaves the author's own understanding.
For AI code, "nobody reviewed it" leaves nothing — unless the upper bound is mechanical.

The goal of process design: when the human does not get to review in time, the automatic checks still give a non-trivial guarantee.

**Scenario**: Friday afternoon, 14 AI-generated PRs in the queue, the reviewer gets through 5

```
RSY-style (human review is the only gate)
   the remaining 9 PRs: either wait until next week (⊤: know nothing, deliver nothing)
                        or merge blind (know nothing, but deliver)

Bilateral-style (mechanical upper bound first)
   all 14 PRs have already passed:
     · mypy --strict
     · import-linter architecture rules
     · property tests + 100% coverage of changed lines
     · no new type: ignore / skip
   the remaining 9 PRs merge into staging with an explicit guarantee:
     "we do not know whether they do the right thing, but we know they crossed no constraint"
   human review follows next week, checking the lower bound (did it do what was asked)
```

**Lecture concept**: lecture 07, RSY returns ⊤ on timeout, Bilateral returns a non-trivial upper bound.

**What to do**: CI applies a `mechanically-verified` label automatically; the merge policy routes by label + risk tier
instead of a blanket "must be human-reviewed".

---

### 7. False alarms became cheap, so you can move towards sound

**Structural difference**

The whole precision-vs-cost trade-off of lecture 02 carries a hidden cost: the cost of a false alarm.
For human code, one false alarm = one engineer blocked for an hour and annoyed.
That is why static analysis tools in industry have long been tuned very loose.

Regenerating AI code is almost free. The cost of a false alarm drops from "an hour of a person" to "run it again".
The optimum on the precision/cost curve moves: you can use a coarser, more conservative domain with more false alarms, because a wrong block does not hurt.

This is the most counter-intuitive and most actionable AI-specific conclusion the lecture framework yields.

**Scenario**: an 80,000-line legacy Python project; turning on `mypy --strict` reports 2,000 errors, about half of them false alarms

- **Human team**: never turned on strict. 2,000 errors = three engineers for two weeks, half of it changing things that "were fine anyway".
  The false-alarm cost is too high, so they chose a coarser domain and accepted more real bugs slipping through.
- **AI**: let the agent clear all 2,000 in an afternoon (false alarms included, since clearing one costs seconds).
  From then on strict is the bar for every AI PR.

The same logic applies to every rule that is "too pedantic for humans":

```
   rule                                     for humans                    for AI
   ───────────────────────────────────────────────────────────────────────────────────
   functions ≤ 40 lines                     often bypassed / turned off   simply followed
   no except Exception:                     legitimate exceptions, argument   writes specific exceptions
   mutation score ≥ 90%                     nobody wants to maintain it   let it grind
   every public function has a property test   a week is not enough       an hour
```

**Lecture concept**: the precision-vs-cost trade-off of lectures 02/05. The optimum on the curve is set by the cost of a false alarm,
and that cost just dropped by two orders of magnitude.

**What to do**: turn on, for AI code, every check you would not dare turn on for humans. You can now afford a more conservative domain.

---

## Differences of degree (quantitative only, do not over-read)

- **Speed**: code is produced faster than a human's abstraction can keep up. True, but it only amplifies point 1.
- **Widening overshoots**: AI tends to over-generalise and over-write. So do humans.
  It is just that the AI's pace means narrowing (simplification) never happens unless explicitly scheduled.
  Narrowing is anytime — stopping at any step is sound, so it need not be done in one go.
- **Worklist / dependency graph**: needed for any code. AI just brings "too many changes to re-review everything" sooner.

---

## Summary: what the human's job becomes

```
   ① choose the abstract domain          decide what to see, and what you allow yourself not to see   (02/05)
   ② define the invariant I              what every change must preserve                              (01/02)
   ③ build the automatic check for Next(I) ⊆ I    squeeze from both sides: tests + constraints        (01/07/08)
   ④ review the composite result at the granularity of intent, not the diff                           (07)
   ⑤ apply the reduction operator after every step, so redundant representations do not accumulate  (05/03b)
   ⑥ handle hyperproperties separately, do not rely on tests                                          (09)
   ⑦ maintain the dependency graph so the impact radius is computable                                 (03a)
```

The one-sentence version:

```
   the lectures' subject   : programs with no author to ask
   human-written code      : has an author, so much of the machinery can slack off and let the brain fill in
   AI-generated code       : has an "author" who holds no abstraction, remembers nothing across sessions,
                             optimises against the checks, and can be regenerated for free

   ⟹ every mechanism the lectures designed for the "worst case" becomes the default case for AI code
   ⟹ and cheap false alarms let you afford stricter checks than you would apply to humans
```

Be honest about the cost: every mechanism in the lectures shares one cost — false alarms. Type systems reject correct code,
architecture rules block reasonable exceptions. Lecture 02 says this is a consequence of Rice's theorem, not of tools being bad.
The cost of "controlling AI" is not zero; the cost is accepting a certain fraction of "it was actually fine but got blocked".
That fraction is what you tune in ① by choosing the abstract domain.

---

## Appendix: symbols

```
  ⊑ ⊒     partial order (precision)     ⊔ ⨆     join (least upper bound)
  ⊓ ⨅     meet (greatest lower bound)   ⊥ / ⊤   bottom / top
  α       abstraction function          γ       concretisation function
  ∇       widening operator             ρ       reduction operator
  f#      abstract transfer function    lfp     least fixed point
  [x # w] x is independent of w's initial value    =_L   equal on low variables
```
