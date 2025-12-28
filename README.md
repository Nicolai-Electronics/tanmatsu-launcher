# Tanmatsu launcher

[![Build](https://github.com/Nicolai-Electronics/tanmatsu-launcher/actions/workflows/build.yml/badge.svg)](https://github.com/Nicolai-Electronics/tanmatsu-launcher/actions/workflows/build.yml)

A launcher firmware for ESP32 based devices which allows users to configure WiFi, browse apps from an online repository and download and run apps on their devices.

This application supports the following boards:

 - Tanmatsu (including Konsool and Hackerhotel 2026 variants)
 - MCH2022 badge
 - Hackerhotel 2024 badge
 - ESP32-P4 function EV board


## License

This project is made available under the terms of the [MIT license](LICENSE).

# Documentation
For help with launching and finding documetation please visit [our documentation website](https://docs.tanmatsu.cloud/software/)

# Getting started Arch / Manjaro
Make sure your user has access rights to the usb port
```
sudo usermod -aG uucp erik
```
Log your user out and back in after this command:

To get started using the firmware please execute the following commands:
```
git clone https://github.com/Nicolai-Electronics/tanmatsu-launcher.git
cd tanmatsu-launcher
make prepare
make build
make flashmonitor
```