# Colors: map colors and chart color scales

There are two kinds of colors you can change, in two places.

## Map colors (Settings > Colors)

The colors of the **radar and map screens**: cities, counties, county labels, highways and secondary roads, lakes and rivers,
the location dot, the observation dots, the map background, the warning and watch colors (tornado, thunderstorm, flash flood,
special marine, special weather statement, snow squall, dust storm), the mesoscale discussion colors, wind barbs, and the colors of
the forecast icon on the home screen. Each row has a color patch and a name; click the patch to open a color chooser and pick another. The change shows the
next time the screen is drawn. The program starts with white cities, grey counties and a black map background, so you can
always choose those colors again.

## Color scales of the model charts (the Model Viewer's "Colors...")

A model chart's fill is painted with a **color scale**: a list of stops, each a value and a color, with the colors blending
between stops. "Colors..." in the Model Viewer (and the little color strip beside a fill in the Checks picker) opens the editor
for the scale of the chart you are looking at.

- **Every chart that uses the same default scale changes with it.** Temperature charts share one scale, wind charts another;
  the editor's title says which kind you are changing and its values are in the chart's own units.
- **Double-click a color** to change it. Colors can be partly or fully transparent (the fourth value, alpha): a stop that is clear
  leaves the map showing through, which is how a chart keeps "nothing here" empty.
- **Add stop** and **Remove stop** change the number of stops (a scale needs at least two, and two stops cannot have the same
  value).
- **Flat bands** makes each color hold from its value up to the next stop instead of blending, and draws nothing below the
  first stop.
- **Apply** puts it to use at once: every chart of that kind is drawn with it now.
- **Back to default** removes your scale; the program's own comes back.
- **Import... / Export...** read and write a scale as a text file (`# value red green blue alpha`, one stop a line), so you can
  keep one, share one, or start from another program's. An imported scale's values are in the chart's units: check them before
  pressing Apply.

Your scales are kept in the settings, by the scale they replace; they stay through upgrades.
