# High Precision Input POC

This proof of concept aims to demonstrate the straightforward nature  
of replicating Overwatch's high-precision mouse input within Valve's Counter-Strike  
franchise, specifically focusing on the latest game in the series, CS2.

# Why?

Due to my profound disappointment with Valve and the direction taken with CS2,  
rather than aiming to rival Blizzard's established four-year-old solution,  
the CS2 team opted to bind inputs to the fps.  
I personally consider this decision extremely imprudent,  
especially given the simplicity of implementing this system instead of or alongside of subtick.  
The necessary interpolation components are already encompassed within the subtick system.

# So what does it do precisely?

It leverages raw input to gather mouse data, timestamps it, and stores it in a vector.  
During each tick, it utilizes this data to execute actions in real-time, irrespective of fps/ticks limitations.  
In this POC, the primary focus is on managing shooting mechanics exclusively.
  
Current capabilities of this POC include:
- Precise shot timing upon the initial press of the mouse1 button, independent of the frames per second (fps) or ticks per second (tickrate).
- Accurate simulation of gunfire spray, replicating a 600 rounds per minute (rpm) rate, with shots firing precisely every 100ms without necessitating the wait for the subsequent frame/tick.
  
# Can I implement this in my game?

Absolutely! That's the exact reason for making this open source.  
The gaming world benefits from better input systems.  
However, it's crucial to note that while this serves as a proof of concept,  
a complete implementation will require significantly more code.  
It's not a simple copy-and-paste solution.  
Please adhere to the license terms.  
I'm thrilled if you've found this POC either educational or beneficial.

# Compiling
Utilize Visual Studio 2022 for compilation.  
The .sln file has been provided for your convenience.
