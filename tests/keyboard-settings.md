# In-game keybindings

Use an isolated portable (`-singleuser`) data directory. Open Display settings
and select Keybindings. Never use personal pakset data for these checks.

1. On first launch, verify `pak/config/menuconf.default.tab` matches the original
   `menuconf.tab` byte for byte. Subsequent launches must not overwrite it.
2. Change Query to an unused key. Only that key field in menuconf.tab may change;
   comments, line endings, icons, sounds and toolbar parameters must remain intact.
   Restart and verify the assignment survives. menuconf.tab is the sole active
   source of bindings; menuconf.default.tab stores the initial defaults.
3. Edit the key in menuconf.tab externally while the game runs. Switch away from
   Keybindings and back: the changed key must appear and work without restarting.
   Verify function keys and Ctrl/Shift, including Ctrl+Shift numeric notation.
4. Clear a binding. Verify a zero key in menuconf.tab, Unassigned in the UI, and
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
