Elfin is a terminal-based, from scratch text editor loosely based on 'Kilo' by Salvatore Sanfilippo aka antirez.

IMPORTANT: Your terminal must support RGB color format. You must have a nerd font installed (otherwise you'll need to modify the function "statusPrintMode" in display.c).

# Demo
https://github.com/user-attachments/assets/75fc04b1-6fe5-45b6-9882-5971fe15a358

# Installation
- ``git clone`` this repo
- navigate to this directory, run ``make``
- add the following command to your ~/.zshrc or ~/.bashrc: ```export PATH="/~/elfin:$PATH"```

now you should be able to run anywhere by typing ``elfin <filename>``

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
