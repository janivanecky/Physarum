# Physarum
![title.png](imgs/title.png)

Inspired by [Sage](https://www.sagejenson.com/physarum) (based on [this paper](http://eprints.uwe.ac.uk/15260/1/artl.2010.16.2.pdf)).


## Cool gifs 3D

![params1_3d.png](imgs/params1_3d.png)

![gif1_3d.gif](imgs/gif1_3d.gif)


![params2_3d.png](imgs/params2_3d.png)

![gif2_3d.gif](imgs/gif2_3d.gif)

## Some videos exploring parameter space (links to higher quality videos below)

![vid0.gif](imgs/vid0.gif)

![vid1.gif](imgs/vid1.gif)
![vid2.gif](imgs/vid2.gif)
![vid3.gif](imgs/vid3.gif)
![vid4.gif](imgs/vid4.gif)

[video1](https://twitter.com/i/status/1117963204791951362)
[video2](https://twitter.com/i/status/1117981241947516928)
[video3](https://twitter.com/i/status/1117981391986159616)
[video4](https://twitter.com/i/status/1117994756389216256)
[video5](https://twitter.com/i/status/1117994952804278273)

## [DoF Rendering](https://inconvergent.net/2019/depth-of-field/)

### DoF rendering of particle pairs
![DoF Particle Pairs](imgs/dof_particle_pairs.png)

### DoF rendering of particles
![DoF Particles](imgs/dof_particles.png)

### DoF rendering of trail map
![DoF Trail](imgs/dof_trail.png)

### Vanilla rendering of trail map
![Vanilla Rendering](imgs/standard.png)

## Cool gifs 2D
Slime-mold-ish behavior

![params1.png](imgs/params1.png)

![gif1.gif](imgs/gif1.gif)

Something different, and cooler

![params2.png](imgs/params2.png)

![gif2.gif](imgs/gif2.gif)


# Build Instructions

**Requirements:**
* Visual Studio (preferably 2019 or 2017)
  
**Steps**
1. Clone [Builder repo](https://github.com/janivanecky/builder)
2. Make sure that path in `build.bat` inside Builder repo points to existing `vcvarsall.bat` (depends on your VS installation version)
3. Run `build.bat`
4. Optionally run `setup.bat` to setup PATH - you'll be able to run builder just by using `build` command
5. Clone [cpplib repo](https://github.com/janivanecky/cpplib)
6. Clone this repo
7. Run `build run physarum.build` - if you didn't setup PATH in step 4, you'll have to use `YOUR_BUILDER_REPO_PATH/bin/build.exe` instead

If there are any problems you encounter while building the project, let me know.