
ColorizeGradient Node
=====================


ColorizeGradient generates a texture based on colormaps to assign colors to data values.



![img](../../images/nodes/ColorizeGradient_settings.png)


## Category


Texture
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|alpha|VirtualArray|Texture alpha map.|
|level|VirtualArray|Data values for color selection.|
|noise|VirtualArray|TODO|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture|VirtualTexture|Texture (RGBA).|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|clamp_alpha|Bool|Clamp to [0, 1] to input alpha map.|
|gradient|Color gradient|Colormap as a manually defined color gradient.|
|reverse_alpha|Bool|Reverse the input alpha map.|
|reverse_colormap|Bool|Reverse the colormap range.|

## Example


![img](../../images/nodes/ColorizeGradient_hsd_example.png)

Corresponding Hesiod file: [ColorizeGradient.hsd](../../examples/ColorizeGradient.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


