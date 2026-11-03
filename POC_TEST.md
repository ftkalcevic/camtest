# CAM proof of concept: test script

This tests the Alibre API behaviour the CAM design depends on. Most results go
to a log file, but some steps ask you to note what you see, because the log
can't see the screen.

## 1. Build and install

1. Close Alibre Design.
2. Open `camtest.sln`, choose **Debug | x64**, and build. If the build fails,
   send me the errors. I couldn't run Visual Studio when writing this code.
3. From the project folder, run:

   ```
   powershell -ExecutionPolicy Bypass -File deploy\install.ps1 -Configuration Debug
   ```

4. Delete `%LOCALAPPDATA%\camtest\poc.log` and `poc_keys.txt` if they exist.

## 2. Prepare test files

Use copies, because step 7 edits a part:

- An assembly with at least two parts, with one moved or rotated away from the
  assembly origin. Include a part with a flat face, a through hole, and a pocket
  if you can.
- A separate part for the stock test that is **not** used in this assembly.

## 3. Open the tab

1. Start Alibre Design and open the assembly.
2. On the **CAM Test** ribbon tab, click **CAM Tab**.

**Note:** Does a **CAM** tab appear in the Design Explorer, with a tree, buttons,
and a log list? Does the tree expand and select normally?

A cyan square with one diagonal should appear at the assembly origin.

**Note:** Do you see it? Measure it against a known feature. A 100 mm side means
the graphics window uses centimetres.

## 4. Tab focus

1. Click the Design Explorer's main tab, then click back on **CAM**. Repeat twice.
2. With **CAM** selected, click **Hide toolpaths**, then **Refresh toolpaths**.

**Note:** Does the square disappear when you leave the tab and return when you
come back? Can you still pan, rotate, and zoom while it's showing?

### Display modes

Already answered: **Redraw on every render** is required, so it's now on by
default and you can skip this part. If you want to repeat it, the tab has two check
boxes that change how the square is drawn. For each combination, with the square
visible:

1. Pan, zoom, and rotate the view.
2. Hover over a face so it highlights.
3. Rotate so a part sits in front of the square.

| Draw over the model | Redraw on every render |
|---|---|
| Off | Off |
| On | Off |
| Off | On |
| On | On |

**Note:** For each combination, does the square survive steps 1 and 2? Is it
hidden by the part in step 3? Does the view feel slower?

Use the best combination for the rest of the test.

## 5. Native commands

1. With **CAM** selected and the square visible, start a native Alibre command,
   such as adding a constraint or measuring. Cancel it.
2. Click **Refresh toolpaths** on the tab.
3. Click **Hide toolpaths**, then click **Show Toolpaths** on the ribbon.
4. On the ribbon, click **CAM Tab** to see if it closes the tab, then click it
   again to reopen it.

**Note:** Does the **CAM** tab stay open and selected during the native command?
Does the square disappear? Does each route bring it back, or is the ribbon button
greyed out? Does auto-show ever interrupt a command you were running? What does
clicking **CAM Tab** a second time do?

## 6. Picking

With the square visible and **CAM** selected, click these, one at a time:

1. A flat face.
2. A hole's cylindrical face.
3. A straight edge.
4. The same items on a part you have **rotated**, not just moved.

Alibre's documentation says the vertex filter isn't valid in assemblies. The next
step tests that.

A red cross marks the raw clicked point. A yellow square marks the picked item's
centre, moved by the part's position in the assembly.

**Note:** Can you select faces, edges, and vertices while the view is active? Is
each yellow square on the item you picked, including on the moved part?

Click **Next pick filter**, then try to pick a face, an edge, and a vertex. Repeat
five more times. It steps through faces, edges, parts, vertices, faces/edges/vertices,
then faces and edges.

**Note:** Does each setting allow only what it names? Can you pick a vertex with
either vertex setting? Does Alibre's own filter control change to match? Does
Alibre's own filter offer vertices in an assembly?

If the toolpaths ever disappear while **CAM** stays selected, note what you had just
done. The log now records clicks and keys to help explain it.

Then click **Large polyline on/off** to draw 200,000 points in orange.

**Note:** Does view rotation stay usable? Click the button again to remove it.

## 7. Changes and saved keys

1. Click **Bind saved keys**. Everything should bind at this stage.
2. Drag the moved part to a new position, or apply a constraint.
3. Save the assembly, close it, and close Alibre Design.
4. Restart Alibre Design, open the assembly and the CAM tab, and click **Bind
   saved keys**. This tests the keys across a reopen.
5. Open one picked part in its own window. Change a feature that does **not**
   touch the picked items, such as adding a small hole elsewhere. Save it and
   return to the assembly.
6. Click **Bind saved keys**.
7. Edit the part again, this time changing a dimension of a picked face. Save it
   and return to the assembly.
8. Click **Bind saved keys**.
9. Edit the part again, deleting a feature that made a face you picked. A face
   from a later feature works best, such as a pocket's floor. Pick it first if
   you need to, and note its name. Save it and return to the assembly.
10. Click **Bind saved keys**.

**Note:** Did each edit in steps 5, 7, and 9 update the assembly, or did you have
to rebuild it?

## 8. Stock

1. Click **Test stock load...** and choose the separate part.

**Note:** Does any window open or flicker? Does a green box appear at that part's
coordinates? Does Alibre behave normally afterwards?

## 9. Send the results

Click **Open log file**, or open `%LOCALAPPDATA%\camtest\poc.log`. Send me:

- `poc.log` and `poc_keys.txt`.
- Your answers to each **Note**.
- Whether Alibre crashed or hung, and at which step.

If Alibre crashes, the log still holds everything up to that point.

## To remove it

```
powershell -ExecutionPolicy Bypass -File deploy\uninstall.ps1
```
