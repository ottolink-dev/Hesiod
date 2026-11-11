# Sources used by HighMap FetchContent. Keep revisions aligned with its
# external/*.cmake files; branch-based dependencies are pinned here as well.
{ fetchFromGitHub }: {
  dkm = fetchFromGitHub {
    owner = "genbattle";
    repo = "dkm";
    rev = "7207b7a543868acd8ca069e2ba29cc6a262059ad";
    hash = "sha256-XuCsN3NCUKi57PD7CubgrAwofc10FZd+bHlQaajmipE=";
  };
  fastnoiselite = fetchFromGitHub {
    owner = "Auburn";
    repo = "FastNoiseLite";
    rev = "7ccfbc16eb1c932568f177d63a9ba51d89bbe516";
    hash = "sha256-l4FoG2DXHGf8x72NkZi3rA1MLwOG6yTcvropZ0WFuJY=";
  };
  hmm = fetchFromGitHub {
    owner = "fogleman";
    repo = "hmm";
    rev = "5ea611461e9f8b40f3d97e9f10fcf0f40592030b";
    hash = "sha256-4ZHN5p9EU3JN6y7OpMoTF/Sc9s05ctBhz4sUFlPAuVg=";
  };
  libnpy = fetchFromGitHub {
    owner = "llohse";
    repo = "libnpy";
    rev = "890ea4fcda302a580e633c624c6a63e2a5d422f6";
    hash = "sha256-RF0OL4B8kfC76onT+kV1yKKpu3Os99wPLbDtoI4yZQw=";
  };
  nn-c = fetchFromGitHub {
    owner = "sakov";
    repo = "nn-c";
    rev = "565c0eeb0be8827ab6e614a4169bf98e1c42b779";
    hash = "sha256-OVTDiiYcyKeDA8iCAAkKCsnfG6rHcls/uiOQ6bhJTIQ=";
  };
  mixbox = fetchFromGitHub {
    owner = "scrtwpns";
    repo = "mixbox";
    rev = "a1bdb75a668f638ba066aa74bfd32809ed7fef45";
    hash = "sha256-Lz0Z5lzm+pxBs4xenmCgx0Zo061EGKRncFHYFgzZhQU=";
  };
  noise = fetchFromGitHub {
    owner = "otto-link";
    repo = "Noise";
    rev = "26c375c27169be6c253bdbe702dbb063c9ee6f04";
    hash = "sha256-9t1szkeVF+pN/RcTOmSq1qSmtg75j5oz7yspOnDz1hc=";
  };
  terrain-descriptors = fetchFromGitHub {
    owner = "otto-link";
    repo = "terrain-descriptors";
    rev = "b2aa7bba68019453d2ef819c88f6e75ab6a36f19";
    hash = "sha256-X3NXkGMFiXljxvg7caigrwFjRnzk2trb5VXpU9bmNic=";
  };
  nanoflann = fetchFromGitHub {
    owner = "jlblancoc";
    repo = "nanoflann";
    rev = "92911c0bc382e4b287330219bc720ca2b30b2857";
    hash = "sha256-HNeeMb7T0FyufE3TwzX8WQhREIdE7gZ8l7Q0BQb1P7I=";
  };
  lightusd = fetchFromGitHub {
    owner = "lighttransport";
    repo = "LightUSD";
    rev = "1fdd60a6171724b5ce0e6716c25b2bba1aed7ad3";
    hash = "sha256-abBpE0FVRl9POJ1jtfX3nGwfGPUFBJduCxDT+Q+s8+s=";
    fetchSubmodules = true;
  };
}
