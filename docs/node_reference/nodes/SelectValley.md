
SelectValley Node
=================


Identifies and selects valley-like regions in the heightmap based on curvature analysis. The selection can be inverted, and additional parameters allow fine-tuning of the selection process. The output is a mask representing the relative width of the valley. The value is 1 at the valley center and decreases to 0 at the edges of the valley.



![img](../../images/nodes/SelectValley_settings.png)


## Category


Terrain Features/Selector
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Heightmap data used as input for valley detection.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|A mask representing the relative width of the valley. The value is 1 at the valley center and decreases to 0 at the edges of the valley.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|radius|Float|Defines the search radius for valley detection, controlling how localized or broad the selection is.|
|ridge_select|Bool|If enabled, selects ridges instead of valleys.|

## Example


![img](../../images/nodes/SelectValley_hsd_example.png)

Corresponding Hesiod file: [SelectValley.hsd](../../examples/SelectValley.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


