# Blender Bridge

![img](../../Hesiod/data/hesiod_icon_64.png) ![img](images/blender_logo_64.png)

The Blender Bridge allows Hesiod to stream terrain data directly into Blender for visualization and further editing. This tutorial explains how to install the Blender add-on and establish a connection between Hesiod and Blender.

The add-on script is located at: [bridges/streamer_addon/hesiod_streamer.py](streamer_addon/hesiod_streamer.py)

## Intalling the Add-on in Blender

- Launch Blender and open the Preferences window: Edit → Preferences

![img](images/tuto00.jpg)

- Open the “Add-ons” tab

- Click “Install from Disk...” (top-right)

![img](images/tuto01.jpg)

- Navigate to: ```bridges/Blender/streamer_addon/hesiod_streamer.py```

- Select: ```hesiod_streamer.py```

- Once installed, ensure that the add-on is enabled

![img](images/tuto02.jpg)

- Return to the main Blender viewport and open the sidebar by pressing N (if it is not already visible).

![img](images/tuto03.jpg)

- Click the Connect button to establish communication with Hesiod. If Blender is started before Hesiod, the initial connection attempt may fail. Once Hesiod is running, simply press Connect again to reconnect.

![img](images/tuto04.jpg)

## Streaming Data from Hesiod

To stream terrain data to Blender:

- Add a BlenderBridge node to your graph.
- Connect a heightmap and/or texture to the node inputs.
- Optional: update the graph.

The bridge automatically creates:

- A mesh generated from the heightmap
- A default material using the streamed texture

Once created, the Blender object can be manipulated normally.

Subsequent graph updates only modify the terrain elevation and texture data, preserving the object's transform and other Blender-side edits.

![img](images/tuto05.jpg)

## Persistence

The connection between Hesiod and Blender is preserved when saving and reopening both Blender scenes and Hesiod projects, provided that the generated Blender object keeps the same name.

After loading a scene, it is recommended to verify that all bridge connections have been restored correctly before continuing to work.

