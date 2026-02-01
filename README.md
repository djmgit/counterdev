# Counterdev: Linux character device driver module to count!

This is a toy kernel module which implements a counter. No real device is involved

## Build

Make sure you have the linux kernel headers corresponding to your kernel version installed.

Open terminal and run:

```
make
```

## Running the module

Open terminal and run:

```
sudo insmod counterdev.ko
```

Make sure /dev/counterdev is created

Check dmesg logs using

```
sudo dmesg | tail -n 20
```

Its recommended that you build the module on your system first.

## Count

Writing any number to the device file increases the counter by that amount.

```
echo 20 | sudo tee -a /dev/counterdev
echo 100 | sudo tee -a /dev/counterdev
```

Reading from the device file will give the current value of the counter.

```
sudo cat /dev/counterdev

# 120
```

If the value of the counter crosses 1000, it will wrap around from zero.

Only one instance of the device file can be open at any point in time.

## Removing the module

When you are done playing with the module, you can remove the module using:

```
sudo rmmod /dev/counterdev
```

This will also remove the device file.
