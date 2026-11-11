import json
import os

HSD_DATA_PATH = "Hesiod/data"

STD_PARAM = {
    "seed":
    "Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.",
    "kw":
    "Base spatial frequencies in the X and Y directions. The frequencies are defined with respect to the entire domain: for example, kw = 2 produces two full oscillations across the domain width (and similarly for the Y direction).",
    "lacunarity":
    "The frequency scaling factor for successive noise octaves. Higher values increase the frequency of each successive octave.",
    "octaves":
    "The number of octaves for fractal noise generation. More octaves add finer details to the terrain.",
    "persistence":
    "The amplitude scaling factor for subsequent noise octaves. Lower values reduce the contribution of higher octaves.",
    "weight":
    "Controls how much higher FBM octaves contribute to the noise based on local elevation. A higher weight suppresses high-frequency octaves at low elevations and increases their influence at higher elevations, producing terrain where fine details appear mainly near peaks while lower areas remain smoother.",
    #
    "dn_add_default_noise":
    "Enables or disables the built-in drainage noise. If an external noise input is provided, it overrides this default noise.",
    "dn_kw":
    "Base spatial frequencies in the X and Y directions.",
    "dn_noise_amp":
    "Noise amplitude.",
    "dn_noise_type":
    "Noise type.",
    "dn_seed":
    "Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.",
    "dn_smoothness":
    "Controls the resulting smoothness of the fractal layering process.",
    #
    "post_gamma":
    "Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.",
    "post_gain":
    "Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.",
    "post_inverse":
    "Inverts the output values after processing, flipping low and high values across the midrange.",
    "post_mix":
    "Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.",
    "post_mix_method":
    "Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.",
    "post_remap":
    "Linearly remaps the output values to a specified target range (default is [0, 1]).",
    "post_saturate":
    "Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.",
    "post_smoothing_radius":
    "Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.",
    #
    "mask_clamp":
    "Clamps the mask values to the range [0, 1].",
    "mask_normalize":
    "Normalizes the mask values to the range [0, 1] using standard deviation clipping.",
    "mask_activate":
    "Enables or disables the internal mask. If the node's 'mask' input is connected, this setting is bypassed and the input mask is used instead.",
    "mask_gain":
    "Controls the intensity or influence of the internal mask. Bypassed if the 'mask' input is connected.",
    "mask_inverse":
    "Inverts the internal mask, applying the operator where the mask is low. Ignored if a 'mask' input is provided.",
    "mask_radius":
    "Defines the smoothing radius for the internal mask. A value of 0 disables smoothing. This is bypassed if the 'mask' input is used.",
    "mask_type":
    "Specifies how the internal mask is computed: 'Elevation' uses height, 'Gradient Norm' uses slope, and 'Elevation mid-range' selects the middle portion of the height range. This parameter is ignored when a 'mask' input is connected.",
    #
    "solver_stride":
    "Grid sampling stride used by the solver. Higher values process the snow field at a lower spatial resolution, reducing computation time at the cost of fine detail."
}

STD_PORTS = {
    "water_depth":
    "Output water depth map representing flooded areas.",
    "dx":
    "Displacement with respect to the domain size (x-direction).",
    "dy":
    "Displacement with respect to the domain size (y-direction).",
    "envelope":
    "Heightmap used as a post-process amplitude multiplier for the generated noise."
}


def load_node_data():
    """Load node documentation data from a JSON file."""
    fname = os.path.join(HSD_DATA_PATH, "node_documentation.json")
    with open(fname, "r") as f:
        return json.load(f)


if __name__ == "__main__":
    data = load_node_data()

    for k, v in data.items():
        print(k)

        # make sure "standard" descriptions are set
        if v["parameters"] is not None:
            for kp, vp in v["parameters"].items():
                if kp in STD_PARAM.keys():
                    data[k]["parameters"][kp]["description"] = STD_PARAM[kp]

        if v["ports"] is not None:
            for kp, vp in v["ports"].items():
                if kp in STD_PORTS.keys():
                    data[k]["ports"][kp]["description"] = STD_PORTS[kp]
                    print(kp)

    with open(os.path.join(HSD_DATA_PATH, "node_documentation.json"),
              'w') as f:
        json.dump(data, f)
