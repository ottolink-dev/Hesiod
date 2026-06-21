
ColorAdjust Node
================


Applies color correction and tonemapping to a texture, including levels, exposure, contrast, saturation, temperature, gamma, and optional dithering.



![img](../../images/nodes/ColorAdjust_settings.png)


## Category


Texture
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture_in|VirtualTexture|No description|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture_out|VirtualTexture|No description|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|ACES Tonemap|Bool|Applies ACES filmic tonemapping for high dynamic range compression.|
|AGX Tonemap|Bool|Applies AGX tonemapping with smooth contrast and saturation compression.|
|Contrast|Float|Adjusts contrast around the midrange (0.5).|
|Dither Amplitude|Float|Amount of noise added.|
|Exposure|Float|Applies exposure adjustment in stops (power-of-two scaling).|
|Filmic Tonemap|Bool|Applies simple filmic (Reinhard) tonemapping.|
|Gamma|Float|Applies gamma correction (power-law transformation).|
|Levels|Value range|Defines input value range remapping before processing (min/max normalization).|
|Saturation|Float|Controls color intensity (0 = grayscale, 1 = original).|
|Temperature|Float|Shifts color balance between warm (red) and cool (blue) tones.|

## Example


!!! note "No example yet"
    No example available for this node.  
