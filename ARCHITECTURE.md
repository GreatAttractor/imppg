# Unit/integration test naming

Pattern 1: Preconditions_Scenario_Result
Pattern 2: Scenario_Result
Pattern 3: Scenario

Keep the text short, avoid unnecessary words. Instead of:

`GivenTwoFiles_WhenFilteredByName_ThenMatchingFileListed`

use:

`TwoFiles_FilterByName_MatchingFileListed`

Prefer patterns 2 & 3 if preconditions and result are obvious and same for the whole suite/test group.


# Scripting API naming conventions

Typically script authors will use property setters more often than the getters. Therefore use setter names without a prefix, and prefix getters with `get`, e.g.:
```Lua
s = imppg.new_settings()
s:unsh_mask_sigma(1.5)
sigma = s:get_unsh_mask_sigma()
```
