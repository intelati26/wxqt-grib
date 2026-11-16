# Help coverage: everything a person can edit has an explainer

The rule (the maintainer, 2026-11-15): **anything a person can edit has a help page in the program**, written from the same
file as the documentation so there is one text to keep right. This is the checklist. A row is done when the in-app Help
(the toolbar's "Help and further reading") has a page for it and the place where it is edited points there.

Help window pages today: **Models** (one page for each model, `help/models/<id>.md`), **Editing the program's data** (the rules,
Servers, Chart files, Model files), **Settings and customizing** (Colors; Checks, presets, favorites, saved views; Home screen
order and the toolbar; Saved locations; Local model runs; GDAL and NetCDF files; Accounts and keys), **Adding a model**, **Model
sources**. The guides are `docs/*.md` bundled by `help/help.qrc` (`help/guides/<name>.md` is `docs/<name>.md`).

| editable thing | where it is edited | text where it is edited | Help window page | status |
|---|---|---|---|---|
| Server addresses (`servers.json`, the Servers table) | Settings > Model Sources | explanation, tooltips, "Help..." button | *Servers*, *The rules* | done |
| Model files (`.model`) | the models folder | tooltips on "Your file..." | *Model files*, *Adding a model* | done |
| Chart files (`.chart.json`) | the models folder | - | *Chart files* | done (the Help window only; the Model Viewer does not mention "Your charts" yet) |
| The catalogue check (what to do with Gone / Not cataloged) | `WXQT_OPEN=catalogcheck:` | - | *Model sources*, *The rules* | done (no screen yet) |
| Colour scales (the Colors editor, `PaletteEditor`, Settings > Colors) | Settings, the Model Viewer's "Colors..." | 2 explanatory labels in the editor, none in Settings > Colors | *Colors* | done (Help window only) |
| Checks presets, favorites, saved views (the Checks picker, "Saved views") | the Model Viewer | 6 tooltips, 2 labels | *Checks, presets, favorites, saved views* | done (Help window only) |
| Home screen order and layout (zones, the forecast column, the toolbar order and style) | Settings > Home Screen Order, Toolbar Order | 5 labels, 1 tooltip; the layout editor has none | *Home screen order and the toolbar* | done (Help window only) |
| Saved locations | Settings > Locations | a few labels | *Saved locations* | done (Help window only) |
| Local model runs (ArWen, WOOF-NAM folders) | Settings > Local runs | 2 labels, 1 tooltip | *Local model runs* | done (Help window only) |
| GDAL and NetCDF settings | Settings > GDAL | 4 labels, 1 tooltip | *GDAL and NetCDF files* | done (Help window only) |
| Accounts (contact email, keys) | Settings > Accounts | 2 labels | *Accounts and keys* | done (Help window only) |

"Text where it is edited" is what is on the screen (a tooltip, an explanatory label): it exists for most and is short. A Help
page is the longer explainer with the rules, the examples and what goes wrong. Each row has one Markdown page in `docs/` (so
it is also documentation), listed in `help/help.qrc` and in the tree of `HelpViewer`; `tests/gfs/run_help.sh`
checks that every page the tree names is bundled and that the links between pages resolve.

When a new thing becomes editable (a new file kind, a new Settings table, a new editor), **add its row here and its page in
the same change**.

**Help buttons (2026-11-16):** every Settings tab has a "Help..." in the corner of the tabs that opens the page about the tab on show
(`SettingsMain`: Colors, Locations, Toolbar and Home Screen Order, Model Sources, Local Runs, GDAL, Accounts; General, Map and About have no page of
their own), the Colors editor (`PaletteEditor`) and the Checks picker have one of their own (`HelpViewer::button`), the Servers table already had.
**Still to do:** the pages were written from the program's own on-screen text and code: where a page says what a control does,
check it against the screen when that screen next changes.
