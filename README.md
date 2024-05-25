# Pathfinder

![Screenshot (207)](https://github.com/brayshi/pathfinder/assets/83468661/0643bbe3-6b38-4916-b38a-9734ab74bfc5)

Pathfinder is an innovative platformer that empowers players with creative control over their environment. Set in a colourful world filled with challenging obstacles, players embark on a journey where imagination shapes the gameplay.

In Pathfinder, players can harness the power of a magical pencil to draw lines directly onto the game world. These lines serve as both platforms to aid in traversal and strategic blockades to thwart enemies and overcome hazards. Whether it's bridging chasms, creating staircases to reach new heights, or blocking incoming projectiles, every line drawn is a stroke of creativity with tangible consequences.

![Screenshot (209)](https://github.com/brayshi/pathfinder/assets/83468661/dca7fd04-2a83-44b7-9470-40f73a3633e7)

With intuitive controls and dynamic physics, Pathfinder offers a seamless blend of platforming action and creative expression. Players must think critically and adapt their drawing strategy on the fly to navigate treacherous terrain, outsmart cunning enemies, and uncover secrets hidden within the vibrant landscapes.

Embark on an adventure where the power of imagination is the ultimate tool. Dive into Pathfinder and let your creativity run wild as you sketch your path to victory in this charming and inventive platformer.

# Table of Contents
1. [Requirements](#requirements)
2. [Quickstart](#quickstart)
3. [Credits](#credits)
4. [FAQ](#faq)

# Requirements
- Visual Studio (2019 or higher)
  - Visual Studio Microsoft C/C++ Extension
  - [OpenGL](https://www.opengl.org/)

# Quickstart
To get started, first download this repository with your chosen method.

> Clone with git
```sh
$ git clone https://github.com/brayshi/pathfinder.git path/to/local/pathfinder
$ cd pathfinder
```

> Download from Github
- Navigate to the `Code` dropdown button
- In the dropdown, click `Download ZIP`
- Extract the downloaded `pathfinder.zip` file

> Clone with Github Desktop
- Use `Clone Repository` under the `file` dropdown (Or using keybinds `Ctrl+Shift+O`)
- Switch to the `URL` tab
- Fill in the URL as `https://github.com/brayshi/pathfinder.git` or `brayshi/pathfinder`

Once downloaded, switch the executable in Visual Studio to pathfinder.exe, as this is the main executable for starting the game.

Click f5 or press the green play button on the upper tab, and the game will build and start!

# Credits

|Name|
|-|
|Aaron Zhang|
|Allan Jiang|
|Brayden Shinkawa|
|Daniel Lee|
|Joshua Chew|
|Kevin Zhu|

# FAQ

Q: My executable isn't able to build because some functions/libraries/files aren't linked or cannot be found. How do I fix this?

A: LNK errors in CMake typically indicate issues with linking libraries or resolving symbols during the build process. What most likely will fix this problem is going to Visual Studio's top tab, clicking Project -> Configure Pathfinder. This will reconfigure the project to notice new libraries that were previously not registered in the project. Try pressing f5 and the project should build now!
