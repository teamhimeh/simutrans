# In-game keybindings

Use an isolated portable (`-singleuser`) data directory. Open Display settings
and select Keybindings. Never use personal pakset data for these checks.

Run `python tests/test_keybindings_unicode.py --cxx g++` with a C++17 compiler
for focused tests of the production file and key parsing functions. These cover
ASCII/Japanese paths, temporary-file cleanup after failed writes/replacement,
backup preservation, direct UTF-8 keys, modifiers and readable/numeric round trips.

1. Use a pakset directory containing Japanese characters. Launch without a
   backup: neither startup nor opening Display settings on a different tab may
   create `pak/config/menuconf.default.tab`. Open Keybindings: it must create
   a byte-identical copy of the current `menuconf.tab`. Change a binding, reopen
   the tab, and restart: the existing backup bytes and modification time must
   remain unchanged. Opening Keybindings must preserve them too. Repeat with
   an ASCII directory name. A backup path that cannot be read or inspected must
   report failure rather than overwrite it. If the backup is removed while the
   tab is open, saving must fail without recreating it; reopening the tab is
   required to prepare a new backup.
2. Change Query to an unused key. Only that key field in menuconf.tab may change;
   comments, line endings, icons, sounds and toolbar parameters must remain intact.
   Restart and verify the assignment survives. menuconf.tab is the sole active
   source of bindings; menuconf.default.tab stores the initial defaults.
3. Edit the key in menuconf.tab externally while the game runs. Switch away from
   Keybindings and back: the changed key must appear and work without restarting.
   Verify function keys and Ctrl/Shift, including Ctrl+Shift numeric notation.
   Also check direct UTF-8 keys such as `あ` and `^+あ`, both at the end of an
   entry and before a toolbar parameter. Japanese comments must be preserved.
4. Assign `]` and verify it is saved as `]`, not `#93`. Check `^A`, `+A`, `F15`,
   `SPACE`, `COMMA` and UTF-8 characters. Reassign an existing numeric binding
   to the same key: its edited field must use the readable spelling too.
   Clear a binding. Verify an empty key field in menuconf.tab, Unassigned in the UI, and
   persistence after reopening the tab and restarting.
5. Restore default keys. Verify keys from the first-run backup are applied to
   menuconf.tab and gameplay; non-key menu settings and backup bytes stay intact.
6. Make menuconf.tab read-only. Changing or clearing must report failure, leave
   both the binding and file unchanged, and preserve the backup. Remove read-only.
7. Assign another operation's key: a warning must identify the key and operation.
   Repeated keys must not open extra warnings or invoke shortcuts. OK, Enter and
   Esc close the warning while leaving Display settings open. No file is changed.
8. Check reserved keys, Esc cancellation, changing tabs during capture, scrolling,
   Japanese/English labels and resized windows. Builder templates requiring a
   parameter must not appear. Every row has a description, key, Change and Clear.

Runtime rereading applies keys to existing operations. Structural menu changes
(new tools, reordered toolbar entries, icons and parameters) require a restart.
