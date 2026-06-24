
MixTexture Node
===============


MixTexture enables the seamless integration of multiple textures by utilizing the alpha channel information to control the blending.



![img](../../images/nodes/MixTexture_settings.png)


## Category


Texture
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture1|VirtualTexture|Input texture.|
|texture2|VirtualTexture|Input texture.|
|texture3|VirtualTexture|Input texture.|
|texture4|VirtualTexture|Input texture.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture|VirtualTexture|Output texture.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|reset_output_alpha|Bool|Reset the output alpha channel to 1 (opaque).|
|use_sqrt_avg|Bool|Use square averaging for alpha-compositing (instead of linear averaging).|

## Example


![img](../../images/nodes/MixTexture_hsd_example.png)

Corresponding Hesiod file: [MixTexture.hsd](../../examples/MixTexture.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
