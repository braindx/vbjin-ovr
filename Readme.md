# VBjin-OVR - A Virtual Boy Emulator for the Oculus Rift

VBjin-OVR is a Virtual Boy Emulator for Windows with support for the Oculus Rift VR display device. It's licensed under the terms of the GNU GPL v2, as is its predecessor, [vbjin].

### Latest Version
3.0 (Oculus PC SDK 1.3.0, compatible with the CV1)

### Getting Started
1. Download the [latest release] and unzip it
2. Make sure you have 'Unknown Sources' enabled in your Oculus Rift settings. To do this, check out the [Oculus support page on the subject]
3. Run VBjin-OVR.exe
4. Gamepad controls can be configured under Config -> Input Config
5. Go to File->Open Rom, and choose a ROM
6. Put on your CV1

### Configuration
1. The color can be changed from the traditional red to greyscale under View -> Coloring.
2. The viewing mode defaults to 'Head Locked' but can be set to 'Immersive' under View -> Oculus Rift Mode

### Oculus Rift Mode: Head Locked
In this mode, the screen is fixed in view and head tracking is ignored.

### Oculus Rift Mode: Immersive
In this mode, the screen is placed in the world and head tracking is used to allow you to look around. This mode should be considered experimental and suffers from artifacts inherent to the Virtual Boy's implementation. For example, rolling your head side to side will break the illusion because the Virtual Boy only separates the left and right eyes horizontally.

[latest release]: <https://github.com/braindx/vbjin-ovr>
[vbjin]: <https://code.google.com/archive/p/vbjin>
[Oculus support page on the subject]: <https://support.oculus.com/878170922281071>