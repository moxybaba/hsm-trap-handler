# Notes from reviewing OpenSBI and the SpacemiT K1 HSM backend

## What changed in my model

My first draft treated hart_stop as though one hart could stop an arbitrary
target hart. Reviewing the SBI HSM interfaces and the SpacemiT backend
changed that understanding.

The K1 start hook receives a hart ID and explicitly wakes that target.
The stop hook has no target parameter and operates on current_hartid(), so
it is a self-stop operation. I changed the cross-hart test accordingly:
the contention case is now two requesters trying to start the same stopped
target hart.

## Why the model uses a single legal-transition function

I wanted invalid transitions to be explicit. A transition is permitted only
when the current state and event match one defined lifecycle edge. Otherwise
the state remains unchanged and the caller receives an error.

This makes negative cases directly testable rather than leaving them as
unwritten assumptions.

## What is still not proven

The dual-start test currently executes sequentially, so it verifies the
state-machine rule but not a physical simultaneous-hart race. OpenSBI uses
atomic compare-and-exchange and a start ticket for that real implementation
problem. Hardware or threaded testing is still needed to exercise that path.

## What I want to compare on K1 hardware

Once cloud access is available, I want to record the state and
acknowledgement behavior for:

1. one suspended target hart receiving a wake/broadcast event;
2. two suspended target harts receiving the same event;
3. repeated start and wake requests; and
4. unexpected or out-of-order completion-style events where the platform
   makes them observable.
