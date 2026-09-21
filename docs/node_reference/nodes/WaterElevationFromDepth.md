
WaterElevationFromDepth Node
============================


Computes the absolute water elevation by adding the terrain elevation and the water depth. This simplifies workflows where only depth is available. The resulting water elevation can be used, for example, when exporting water as an asset with ExportAsset.



![img](../../images/nodes/WaterElevationFromDepth_settings.png)


## Category


Hydrology
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|elevation|VirtualArray|Input terrain elevation values.|
|water_depth|VirtualArray|Output water depth map representing flooded areas.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|water_elevation|VirtualArray|Output absolute water elevation computed as elevation + water_depth.|

## Example


![img](../../images/nodes/WaterElevationFromDepth_hsd_example.png)

Corresponding Hesiod file: [WaterElevationFromDepth.hsd](../../examples/WaterElevationFromDepth.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


