- [ ] Only trigger timers in later_posix.cpp and later_win32.cpp if we're at the global loop
- [ ] User timer in later_win32.cpp is pretty inefficient, have it stop waking so much

## Open questions

* Should private loops have explicit or implicit/automatic extent? i.e. once a private loop is created, is it manually destroyed or is it "garbage collected"? (The latter will need extra work, if native code on background thread needs to schedule work for the private loop--might need handles or explicit reference counting, since cannot rely on R finalizers)
