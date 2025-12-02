rs_bag2image
============

This is convert tool that works on cross-platform (Windows, Linux, MacOS) for RealSense.
This tool converts all data of each stream types (Color, Depth, Infrared, IMU) that contained in bag file to image file and CSV data.

Features
--------
* Extract Color, Depth, and Infrared (Left/Right) streams as images
* Extract IMU data (Gyroscope and Accelerometer) as CSV files
* Save metadata (timestamp, frame number, resolution, format) for all image streams
* Support for latest librealsense2 API

Sample
------
### Command
```
rs_bag2image -b="./file.bag" -s=true -d=true
```
### Output
```
rs_bag2image.exe
file.bag
file
  |-Color
  |   |-000001.jpg
  |   |-000002.jpg
  |   |-metadata.csv
  |
  |-Depth
  |   |-000001.png
  |   |-000002.png
  |   |-metadata.csv
  |
  |-IR
  |   |-000001.jpg
  |   |-000002.jpg
  |   |-metadata.csv
  |
  |-IR_Right
  |   |-000001.jpg
  |   |-000002.jpg
  |   |-metadata.csv
  |
  |-IMU
      |-gyro_data.csv
      |-accel_data.csv
```

Option
------
| option | description                                                                           |
|:------:|:--------------------------------------------------------------------------------------|
| -b     | input bag file path. (requered)                                                       |
| -s     | enable depth scaling for visualization. <code>false</code> is raw 16bit image. (bool) |
| -q     | jpeg encoding quality for color and infrared. [0-100]                                 |
| -d     | display each stream images on window. <code>false</code> is not display. (bool)       |

Environment
-----------
### C++ Tool (rs_bag2image)
* Visual Studio 2015/2017 / GCC 5.3 / Clang (require <code>\<filesystem\></code> supported)
* RealSense SDK 2.x (librealsense v2.x)
* OpenCV 3.4.0 (or later)
* CMake 3.7.2 (latest release is preferred)

### Python Script (images2mp4)
* Python 3.8 or later
* uv (recommended package manager)

Installation
------------
### Using uv (recommended)
```bash
# Install uv if not already installed
curl -LsSf https://astral.sh/uv/install.sh | sh

# Create virtual environment and install dependencies
uv sync

# Run the script
uv run images2mp4 -i ./Color -o output.mp4 -f 30
```

### Using pip
```bash
pip install -r requirements.txt
python images2mp4.py -i ./Color -o output.mp4 -f 30
```

License
-------
Copyright &copy; 2018 Tsukasa SUGIURA  
Distributed under the [MIT License](http://www.opensource.org/licenses/mit-license.php "MIT License | Open Source Initiative").

Contact
-------
* Tsukasa Sugiura  
    * <t.sugiura0204@gmail.com>  
    * <http://unanancyowen.com>  