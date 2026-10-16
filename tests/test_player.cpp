// Host test for MacroPlayer. Build: c++ -std=c++11 -Wall -Wextra -Werror -Iinclude
#include "MacroPlayer.h"
#include <cassert>
#include <iostream>

static const Stroke kCombo[] = {{0x09, 0x06, 0}};  // GUI+C
static const Macro combo = {"COPY", kCombo, 1};
static const Stroke kText[] = {{0x02, 0x0B, 0}, {0, 0x08, 0}, {0, 0x28, 0}};
static const Macro text = {"HE", kText, 3};
static const Stroke kSeq[] = {{0, 0x04, 0}, {0, 0, 100}, {0, 0x05, 0}};
static const Macro seq = {"SEQ", kSeq, 3};
static const Stroke kTrail[] = {{0, 0x04, 0}, {0, 0, 50}};
static const Macro trail = {"TRAIL", kTrail, 2};
static const Macro empty = {"", nullptr, 0};

int main() {
  Stroke out{};
  {  // combo: one stroke, active until the transport is idle again
    MacroPlayer p;
    assert(!p.active() && p.total() == 0 && p.macro() == nullptr);
    assert(p.start(combo, 0));
    assert(p.active() && p.total() == 1 && p.sent() == 0 && p.macro() == &combo);
    assert(p.next(0, true, out));
    assert(out.mods == 0x09 && out.usage == 0x06);
    assert(p.sent() == 1);
    assert(!p.next(1, false, out) && p.active());  // transport busy: still active
    assert(!p.next(2, false, out) && p.active());
    assert(!p.next(30, true, out) && !p.active());  // idle: completes
    assert(p.sent() == 1);
  }
  {  // text: strokes only while transport idle
    MacroPlayer p;
    assert(p.start(text, 0));
    assert(p.next(0, true, out) && out.mods == 0x02 && out.usage == 0x0B);
    assert(!p.next(1, false, out));
    assert(p.next(30, true, out) && out.usage == 0x08);
    assert(p.next(60, true, out) && out.usage == 0x28);
    assert(p.sent() == 3 && p.active());
    assert(!p.next(61, true, out) && !p.active());
  }
  {  // pause timing: starts when transport idle after previous stroke
    MacroPlayer p;
    assert(p.start(seq, 0));
    assert(p.next(0, true, out) && out.usage == 0x04);
    assert(!p.next(10, false, out));      // busy: pause not started
    assert(!p.next(40, true, out));       // pause starts at 40
    assert(p.sent() == 1);
    assert(!p.next(139, true, out));      // 99 ms
    assert(p.sent() == 1);
    assert(p.next(140, true, out) && out.usage == 0x05);
    assert(p.sent() == 3);
  }
  {  // pause waits for idle even if time has long passed
    MacroPlayer p;
    assert(p.start(seq, 0));
    assert(p.next(0, true, out));
    assert(!p.next(500, false, out));
    assert(!p.next(500, true, out));  // pause starts now, not at 0
    assert(!p.next(599, true, out));
    assert(p.next(600, true, out));
  }
  {  // wraparound during pause
    MacroPlayer p;
    assert(p.start(seq, UINT32_MAX - 200));
    assert(p.next(UINT32_MAX - 200, true, out));
    assert(!p.next(UINT32_MAX - 50, true, out));  // pause starts
    assert(!p.next(UINT32_MAX, true, out));       // 50 ms
    assert(!p.next(48, true, out));               // 99 ms across the wrap
    assert(p.next(49, true, out) && out.usage == 0x05);
  }
  {  // trailing pause completes the macro in the same call
    MacroPlayer p;
    assert(p.start(trail, 0));
    assert(p.next(0, true, out));
    assert(!p.next(1, true, out) && p.active());
    assert(!p.next(50, true, out) && p.active());
    assert(!p.next(51, true, out) && !p.active());
    assert(p.sent() == 2);
  }
  {  // abort mid-way stops output, also during a pause
    MacroPlayer p;
    assert(p.start(text, 0));
    assert(p.next(0, true, out));
    p.abort();
    assert(!p.active());
    assert(!p.next(30, true, out) && !p.next(60, true, out));
    assert(p.sent() == 1);
    MacroPlayer q;
    assert(q.start(seq, 0));
    assert(q.next(0, true, out));
    assert(!q.next(10, true, out));
    q.abort();
    assert(!q.next(1000, true, out) && !q.active());
    assert(q.start(combo, 0));  // can restart after abort
    assert(q.next(0, true, out) && out.usage == 0x06);
  }
  {  // start rejected when active or empty
    MacroPlayer p;
    assert(!p.start(empty, 0) && !p.active());
    assert(p.start(combo, 0));
    assert(!p.start(text, 0));
    assert(p.macro() == &combo);
    assert(p.next(0, true, out));
    assert(!p.start(text, 0));  // still active until transport idle
    assert(!p.next(1, true, out));
    assert(p.start(text, 2));  // finished: new macro accepted
    assert(p.total() == 3 && p.sent() == 0);
  }
  std::cout << "Macro player tests passed\n";
}
