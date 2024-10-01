Elfin is a terminal-based, from scratch text editor loosely based on 'Kilo' by Salvatore Sanfilippo aka antirez.

# Installation
- ``git clone`` this repo
- navigate to this directory, run ``make``
- add the following command to your ~/.zshrc or ~/.bashrc: ```export PATH="/~/elfin:$PATH"```

now you should be able to run anywhere by typing ``elfin <filename>``

IMPORTANT: Your terminal should support RGB color format.

# Current Features
- Insert (i/I/o/O/a/A), View (ESC), and Command (:) modes
- Search (/)
- Some basic motions
- Text wrapping
- Text selection (v)
  - Copy/paste (y/p)
  - Delete (d)
- Undo (u)
# Possible Future Features
- Syntax highlighting
