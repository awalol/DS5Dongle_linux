#include <iostream>
#include <fstream>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <dirent.h>

#include "USBGadget.h"

#include <filesystem>

namespace {
constexpr const char* kAudioFunctionName = "uac2.gs0";
constexpr const char* kHidFunctionName = "hid.usb0";
constexpr const char* kAdbFunctionName = "ffs.adb";
}

bool USBGadget::write_file(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << value;
    return file.good();
}

bool USBGadget::make_dir(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

bool USBGadget::remove_dir(const std::string& path) {
    return rmdir(path.c_str()) == 0 || errno == ENOENT;
}

std::string USBGadget::find_udc() {
    DIR* dir = opendir("/sys/class/udc");
    if (!dir) {
        return "";
    }

    dirent* entry;
    std::string udc;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            udc = entry->d_name;
            break;
        }
    }
    closedir(dir);
    return udc;
}

USBGadget::USBGadget(const std::string& name)
    : gadget_root("/sys/kernel/config/usb_gadget/" + name) {}

bool USBGadget::create() {
    if (geteuid() != 0) {
        std::cerr << "需要 root 权限" << std::endl;
        return false;
    }

    udc_name = find_udc();
    if (udc_name.empty()) {
        std::cerr << "未找到 UDC 控制器" << std::endl;
        return false;
    }

    destroy();

    if (!make_dir(gadget_root)) {
        return false;
    }

    if (!write_file(gadget_root + "/idVendor", "0x054C") ||
        !write_file(gadget_root + "/idProduct", "0x0CE6") ||
        !write_file(gadget_root + "/bcdDevice", "0x0100") ||
        !write_file(gadget_root + "/bcdUSB", "0x0200")) {
        return false;
    }

    if (!make_dir(gadget_root + "/strings/0x409") ||
        !write_file(gadget_root + "/strings/0x409/serialnumber", "1234567890") ||
        !write_file(gadget_root + "/strings/0x409/manufacturer", "Sony Interactive Entertainment") ||
        !write_file(gadget_root + "/strings/0x409/product", "DualSense Wireless Controller")) {
        return false;
    }

    if (!make_dir(gadget_root + "/configs/c.1") ||
        !write_file(gadget_root + "/configs/c.1/MaxPower", "500") ||
        !write_file(gadget_root + "/configs/c.1/bmAttributes", "0xC0")) {
        return false;
    }

    // 创建 UAC
    const std::string uac_func = gadget_root + "/functions/" + kAudioFunctionName;
    if (!make_dir(uac_func) ||
        !write_file(uac_func + "/c_chmask", "0x33") ||
        !write_file(uac_func + "/c_ssize", "2") ||
        !write_file(uac_func + "/c_srate", "48000") ||
        !write_file(uac_func + "/function_name", "DualSense Wireless Controller")) {
        return false;
    }

    // 创建 HID
    const std::string hid_func = gadget_root + "/functions/" + kHidFunctionName;
    if (!make_dir(hid_func) ||
        !write_file(hid_func + "/protocol", "0") ||
        !write_file(hid_func + "/subclass", "0") ||
        !write_file(hid_func + "/report_length", "64")) {
        return false;
    }

    std::ofstream report(hid_func + "/report_desc", std::ios::binary);
    if (!report) {
        return false;
    }

    report.write(reinterpret_cast<const std::ostream::char_type *>(desc_hid_report.data()), desc_hid_report.size());
    if (!report.good()) {
        return false;
    }
    report.close();

    // 创建 ADB
    const std::string adb_func = gadget_root + "/functions/" + kAdbFunctionName;
    if (!make_dir(adb_func) ||
        !make_dir("/dev/usb-ffs/") ||
        !make_dir("/dev/usb-ffs/adb")) {
        return false;
    }

    // 有点危险，后面看看有没有办法解决
    system("mount -t functionfs adb /dev/usb-ffs/adb");
    system("adbd &");

    if (symlink(hid_func.c_str(), (gadget_root + "/configs/c.1/" + kHidFunctionName).c_str()) != 0 ||
        symlink(uac_func.c_str(), (gadget_root + "/configs/c.1/" + kAudioFunctionName).c_str()) != 0 ||
        symlink(adb_func.c_str(), (gadget_root + "/configs/c.1/" + kAdbFunctionName).c_str()) != 0) {
        return false;
    }

    sleep(1); // wait for adbd started

    if (!write_file(gadget_root + "/UDC", udc_name)) {
        return false;
    }

    std::cout << "复合设备创建完成：UAC2 + HID" << std::endl;
    return true;
}

void USBGadget::destroy() {
    const std::string udc_path = gadget_root + "/UDC";
    std::ifstream check(udc_path);
    if (check.good()) {
        write_file(udc_path, "");
    }

    // TODO 工厂化设计

    unlink((gadget_root + "/configs/c.1/" + kHidFunctionName).c_str());
    unlink((gadget_root + "/configs/c.1/" + kAudioFunctionName).c_str());
    unlink((gadget_root + "/configs/c.1/" + kAdbFunctionName).c_str());

    remove_dir(gadget_root + "/functions/" + kHidFunctionName);
    remove_dir(gadget_root + "/functions/" + kAudioFunctionName);
    remove_dir(gadget_root + "/functions/" + kAdbFunctionName);
    remove_dir(gadget_root + "/configs/c.1");
    remove_dir(gadget_root + "/strings/0x409");
    remove_dir(gadget_root);
}

bool USBGadget::exists() const {
    if (std::filesystem::exists(gadget_root)) {
        std::ifstream file(gadget_root + "/UDC");
        if (!file.is_open()) {
            return false;
        }
        std::string line;
        if (getline(file,line)) {
            if (!line.empty()) {
                return true;
            }
        }
    }

    return false;
}

/*int main() {
    USBGadget gadget;
    if (!gadget.create()) {
        std::cerr << "创建失败" << std::endl;
        return 1;
    }

    std::cout << "按 Enter 键销毁设备..." << std::endl;
    std::cin.get();
    gadget.destroy();
    return 0;
}*/
