# AqualinkD — AI Development and Review Guidance

**Version:** 1.0.0
**Status:** Initial project-specific guidance
**Scope:** Entire AqualinkD repository

## 1. Purpose

AqualinkD is a long-lived, hardware-facing project that communicates with Jandy/AquaLink pool automation equipment over RS485 and provides control and integration interfaces for other software.

When reviewing or modifying AqualinkD, correctness is not simply:

> "Does this code work for the contributor's installation?"

The important question is:

> "Does this change correctly fit the hardware, protocol, installation, compatibility, existing architecture, and user workflows that AqualinkD already supports?"

AqualinkD has accumulated behavior over many years because of differences between panel generations, protocols, devices, firmware, installations, and external integrations.

AI reviewers must therefore investigate before simplifying, generalizing, or redesigning existing behavior.

---

# 2. Evidence and domain model come before implementation

Before suggesting a code change, establish:

* What hardware is actually involved?
* What panel generation is involved?
* What protocol is being used?
* What does the hardware actually support?
* What does AqualinkD already know about the device or state?
* What existing code paths implement the same or related behavior?
* What evidence demonstrates that the reported behavior is a software defect?
* Does the proposed change apply to all relevant protocols and hardware, or only one combination?
* Is the observed behavior potentially caused by configuration, timing, wiring, bus conditions, or hardware?

Do not infer the answer from variable names, UI appearance, or the contributor's description alone.

Trace the data flow and hardware relationship.

---

# 3. Understand the physical/domain object before changing its representation

Do not assume that something visible to a user represents a corresponding physical object.

For example, AqualinkD can contain virtual buttons or states that do not correspond to a physical circuit or physical button on the RS panel.

Before adding configuration that moves, renames, remaps, or otherwise changes an object, determine:

* Is it physical or virtual?
* Does it correspond to a real panel circuit?
* Is its position meaningful to existing protocol code?
* Does other code depend on its current ordering or representation?
* Would changing its representation alter RS panel behavior?

Do not create configuration machinery to solve a problem that does not exist in AqualinkD's domain model.

---

# 4. Understand the hardware/protocol boundary

AqualinkD does not communicate with one uniform "AquaLink protocol."

Different panel generations, devices, and protocols have different capabilities and limitations.

A feature implemented for one protocol is not automatically an AqualinkD-wide feature.

When reviewing a new capability:

1. Identify the actual hardware capability.
2. Identify which protocols expose or support it.
3. Identify which AqualinkD protocol programmers implement it.
4. Determine which combinations are intentionally unsupported.
5. Implement the feature on all applicable paths.
6. Do not add artificial symmetry where the hardware does not support the feature.

Conversely, do not describe a feature as generally supported when the implementation only supports one applicable protocol.

Protocol-specific functionality can be completely correct when the hardware itself imposes that limitation.

---

# 5. Check existing state before introducing new state

When a protocol implementation discovers information from the RS panel, determine whether AqualinkD already maintains an independent representation of that information.

Examples include:

* pump minimum and maximum speeds
* device configuration
* device identity
* panel state
* configured limits
* protocol-specific capabilities

Do not assume that newly discovered protocol data is simply transient information.

Ask:

* Does AqualinkD already store this?
* Which representation is authoritative?
* Can the values disagree?
* What should happen when they disagree?
* Is the new value configuration, observed state, or a hardware capability?
* Is the change introducing a second source of truth?

A PR that discovers useful hardware information may still be incomplete if it does not reconcile that information with existing AqualinkD state.

---

# 6. Distinguish equipment control from persistent panel configuration

Changing equipment state and changing the persistent configuration stored in an RS panel are different operations.

Before adding code that writes panel configuration, determine:

* Is this ordinary equipment control?
* Is it changing persistent panel configuration?
* Is AqualinkD historically intended to perform this operation automatically?
* Could an automated configuration change have consequences beyond the immediate command?
* Does AqualinkD already expose a safer/manual mechanism for configuring the panel?

Do not automatically turn a discovered configuration capability into an automated configuration feature.

Persistent panel configuration deserves explicit architectural review.

---

# 7. Diagnose the installation before modifying protocol behavior

A reported failure does not automatically mean AqualinkD contains a software bug.

Before changing protocol logic, consider:

1. Incorrect AqualinkD configuration
2. Incorrect panel/protocol selection
3. Incorrect device configuration
4. RS485 timing configuration
5. RS485 bus congestion
6. Dropped or corrupted frames
7. Wiring problems
8. Termination problems
9. Electrical noise
10. Particular panel/device behavior
11. Hardware failure
12. An actual AqualinkD software defect

In particular, do not turn an installation problem into a software feature merely because a software change makes one installation behave better.

For PDA installations, for example, RS485 timing can differ significantly from RS panel behavior. Configuration such as `rs485_frame_delay` may therefore be relevant to diagnosing a problem.

The goal is not to assume the problem is wiring or configuration. The goal is to establish evidence before modifying protocol behavior.

---

# 8. Be suspicious of installation-specific timing assumptions

A measured timing value is not necessarily a hardware or protocol constant.

RS485 timing can depend on:

* number of devices on the bus
* device response behavior
* polling cycles
* other traffic
* panel type
* installation-specific conditions
* configuration
* bus errors or retries

Do not take a value measured on one physical installation and turn it into a universal constant without evidence that it is actually defined by the protocol.

In particular, do not assume that the time required for a keypress, display update, or round trip is fixed simply because it appeared consistent on one panel.

Prefer measuring actual elapsed time at runtime when timing emerges from bus behavior.

Ask:

> "Is this timing a property of the protocol, or a property of the contributor's particular installation?"

---

# 9. Prefer existing architectural integration points

Before introducing a new mechanism, find how similar functionality is already implemented.

AqualinkD has established protocol-programmer and dispatch mechanisms.

If a new operation belongs in an existing abstraction, use that abstraction rather than creating a parallel implementation.

This is especially important for protocol-specific programming such as:

* lights
* pumps
* panel programming
* device control
* menu navigation
* protocol commands

A protocol-specific implementation may be necessary, but it should still fit the established AqualinkD architecture unless there is a concrete reason not to.

Do not bypass an established programmer architecture simply because a separate path can be made to work.

---

# 10. Check dependencies before building on them

Do not build new functionality on top of another PR or change that has not passed testing or has already demonstrated problems.

When a PR depends on another change:

* establish whether the dependency is actually working;
* determine whether its architecture has been accepted;
* avoid compounding an unresolved problem with additional functionality;
* consider whether the smallest useful change can be isolated.

A feature should not become harder to diagnose because several unproven changes have been stacked together.

---

# 11. Preserve working behavior

AqualinkD supports many combinations of hardware and protocols.

When changing one path, identify unaffected paths and ensure they remain unchanged unless the PR intentionally changes them.

In particular, inspect:

* other panel protocols
* other device types
* shared protocol code
* common state
* common packet handling
* API/JSON
* MQTT
* configuration
* simulator behavior

Do not "clean up" unrelated code merely because it is nearby.

Do not assume duplicated-looking code is accidental duplication. It may exist because the underlying protocols or hardware behave differently.

---

# 12. Complexity is a signal, not a verdict

A large or complicated PR is not automatically bad.

Some AqualinkD features genuinely require changes across:

* protocol handling
* device state
* programming interfaces
* API
* MQTT
* discovery
* configuration
* UI
* simulator support

A complex implementation may therefore be appropriate.

However, complexity should trigger investigation:

> Is the complexity required by the hardware and architecture, or is it compensating for an incorrect assumption?

Before accepting a large solution, look for:

* unnecessary prediction
* duplicated state
* new abstractions that duplicate existing ones
* protocol-specific code that bypasses common mechanisms
* functionality unrelated to the requested problem
* a simpler existing state transition where the behavior could naturally belong

Do not reject complexity merely because it is large.

---

# 13. Separate the requested problem from adjacent improvements

Keep the core fix identifiable.

Be cautious when a PR combines the requested fix with:

* scheduling
* optimization
* drift detection
* refactoring
* new configuration systems
* unrelated cleanup
* additional integrations
* speculative future functionality

If the additional functionality is useful, it can often be a separate change.

A smaller first change makes it easier to establish whether the fundamental behavior is correct.

---

# 14. Understand the intended role of utilities

Do not review a utility solely according to its apparent generic or secondary purpose.

AqualinkD contains tools that may be useful outside their original purpose.

For example, a diagnostic/configuration utility may also be useful as a general RS485 traffic monitor. That secondary use does not necessarily define how the utility should behave for AqualinkD users.

Before removing behavior that appears unnecessary, determine:

* Why was it originally added?
* What user workflow does it support?
* Is it part of installation, configuration, discovery, or troubleshooting?
* Does it provide guidance to users who may not understand the underlying system?
* Is the proposed change optimizing for a contributor's secondary use case?

Distinguish between:

* technically necessary behavior;
* historical compatibility behavior;
* user-experience behavior;
* security/permission behavior;
* installation workflow behavior;
* secondary use cases.

Technically unnecessary behavior may still be important to the project's intended workflow.

Likewise, do not preserve behavior merely because it is old. Understand what problem it solves before deciding whether that problem still exists.

---

# 15. Keep middleware integrations generic

AqualinkD provides generic integration interfaces, including MQTT and API endpoints.

The preferred architecture is:

> AqualinkD provides generic capabilities and interfaces; external middleware adapts those interfaces to its own platform.

Do not add substantial direct integrations for individual automation platforms merely because that platform does not support an existing AqualinkD interface.

Before adding platform-specific code, determine:

* Does the MQTT interface already provide the required capability?
* Does the API already provide it?
* Is the requested protocol a generic ecosystem standard?
* Is this actually a thin compatibility adapter?
* Who should own the translation between AqualinkD and the external platform?
* Will this create long-term maintenance coupling?
* Does it create platform-specific semantics that differ from AqualinkD's canonical state?

Generic ecosystem standards can be appropriate.

For example, MQTT Discovery is generic MQTT ecosystem functionality rather than a collection of Home Assistant-specific control paths.

A very thin compatibility interface may also be appropriate when there is a demonstrated ecosystem requirement.

The desired boundary is:

> Keep AqualinkD's core interfaces generic and push platform-specific integration outward whenever practical.

Avoid creating an N×M matrix of direct integrations between AqualinkD and individual middleware platforms.

---

# 16. Treat external interfaces as compatibility surfaces

Changes to the following may affect software outside the AqualinkD repository:

* API endpoints
* JSON structures
* MQTT topics
* MQTT payloads
* MQTT discovery
* configuration formats
* simulator interfaces
* device naming
* externally visible state semantics

Before changing one of these, search the repository for consumers and consider how external projects may depend on it.

Do not assume that because a change is internal to a C function it is invisible to users.

---

# 17. Use history when existing behavior looks strange

AqualinkD is a long-lived project.

If existing code looks unnecessarily complicated, duplicated, or unusual, do not immediately simplify it.

Use:

* `git log`
* `git blame`
* historical PRs
* issues
* protocol documentation
* hardware captures where available

to determine why it exists.

A strange-looking workaround may exist because of:

* an old panel generation
* firmware behavior
* a protocol quirk
* an installation problem that became common
* a device-specific bug
* an interoperability requirement
* a regression that occurred when it was previously changed

The absence of an obvious explanation in the current source is not evidence that the behavior is unnecessary.

---

# 18. Review protocol code defensively

For changes involving RS485 packets or protocol behavior, verify rather than infer:

* packet structure
* command IDs
* device IDs
* packet lengths
* byte positions
* byte ordering
* checksums
* timing
* retries
* response matching
* state transitions
* protocol-specific differences

Where possible, compare implementation against:

* existing protocol implementations
* `JANDY_RS485_PROTOCOL.md`
* captured traffic
* simulator behavior
* known hardware behavior
* historical implementations

A plausible-looking packet is not evidence that it is the correct packet.

---

# 19. Test what the test actually proves

Do not treat the existence of a test as proof that the change is correct.

Ask:

* Does the test exercise the actual changed path?
* Does it test the protocol behavior or only an internal function?
* Does it cover the relevant panel/protocol?
* Does it exercise shared code used by other protocols?
* Does the simulator reproduce the hardware behavior relevant to the change?
* Does the test establish behavior across the supported compatibility matrix?
* Could the test pass while the actual RS485 interaction is still wrong?

A passing test may establish only a small part of the required behavior.

---

# 20. Review the compatibility matrix, not just the changed code

For device or feature changes, explicitly identify the relevant dimensions:

* panel generation
* panel type
* protocol
* device type
* firmware/hardware differences
* configuration
* installation conditions

Then determine which combinations the PR actually changes.

Do not silently generalize:

> "Works on my panel"

into:

> "AqualinkD supports this."

Likewise, do not require implementation in every protocol merely for symmetry when the hardware does not support the feature there.

---

# 21. Distinguish evidence from assumptions

During review, classify conclusions appropriately.

Useful categories include:

### Definite defect

The code contradicts established behavior, an invariant, documented protocol behavior, or demonstrably breaks an existing path.

### Likely defect

There is strong evidence of a problem, but hardware or environmental verification is still desirable.

### Compatibility risk

The change may affect a hardware/protocol/configuration combination not covered by the author's testing.

### Architectural concern

The change conflicts with an established AqualinkD design boundary or creates unnecessary coupling.

### Incomplete implementation

The requested functionality is only partially implemented across relevant protocols, states, or interfaces.

### Question requiring verification

The available evidence is insufficient to determine whether the behavior is correct.

Do not turn an architectural concern or unanswered hardware question into a definite bug merely because it seems plausible.

---

# 22. AI review procedure

When reviewing an AqualinkD change, work through these questions in roughly this order:

1. **What problem is the PR actually solving?**
2. **What hardware and protocol are involved?**
3. **Is the reported problem reproducibly a software problem?**
4. **Could configuration, RS485 timing, wiring, bus conditions, or hardware explain it?**
5. **What does AqualinkD already do for this device/function?**
6. **Where is the existing architectural integration point?**
7. **Which other protocols, panel generations, or devices are affected?**
8. **Does the change introduce a new source of truth?**
9. **Does it change persistent panel configuration or ordinary equipment state?**
10. **Does it alter an external API, MQTT, configuration, or other compatibility surface?**
11. **Does the implementation use actual hardware/protocol evidence?**
12. **What does the testing actually prove?**
13. **What existing behavior could regress?**
14. **Is the complexity justified by the actual problem?**
15. **Is the PR solving the AqualinkD problem, or compensating for an external system/installation?**

Only after those questions should implementation details become the primary focus.

---

# 23. Do not impose generic engineering assumptions on AqualinkD

The following assumptions are specifically unsafe in this project:

* "Duplicated code should always be consolidated."
* "Every protocol should implement every feature."
* "A measured timing value is a hardware constant."
* "A UI-visible object represents physical hardware."
* "A reported failure proves the software is wrong."
* "A technically unnecessary check should be removed."
* "A large PR is necessarily bad."
* "A contributor's secondary use case defines the utility's intended purpose."
* "A direct middleware integration is preferable to a generic API."
* "A passing test means the hardware behavior is correct."
* "A strange existing implementation is probably just bad code."

These may be reasonable questions to ask, but none should be treated as conclusions without project-specific evidence.

---

# 24. What a good AqualinkD PR should demonstrate

A good PR should make it reasonably clear:

* what problem it solves;
* what hardware/protocol is affected;
* why the problem is believed to be in AqualinkD;
* what existing code or architecture it builds upon;
* which compatibility paths are affected;
* which paths are intentionally unaffected;
* what evidence supports protocol/hardware assumptions;
* what testing was performed;
* what the testing does and does not establish.

The goal is not exhaustive paperwork.

The goal is to make important assumptions visible.

---

# 25. Overall review philosophy

AqualinkD should favor:

* evidence over assumptions;
* understanding the hardware before changing the software;
* compatibility over artificial symmetry;
* existing architecture over unnecessary parallel mechanisms;
* generic interfaces over platform-specific coupling;
* runtime measurement over installation-specific timing constants;
* explicit uncertainty over false confidence;
* preservation of working behavior over cleanup for its own sake;
* small, understandable changes where practical;
* but legitimate complexity when the hardware and architecture require it.

The AI's job is to identify problems and risks early.

It is **not** the AI's job to approve or reject a PR by authority.

When uncertain, explain exactly what is known, what is assumed, and what would need to be verified.
