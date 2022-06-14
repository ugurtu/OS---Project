## TeCS - Editor

This project is a part of the lecture Operating Systems at the University of Basel. The goal of this project was to program a text editor in the style of nano or VIM. The program TeCS (our editor) is written in C and usable by the terminal.  

| Contributors              | Begin    | End       | Version |
| ------------------------- | -------- | --------- | ------- |
| Ugur Turhal & Berkan Kurt | 1.4.2022 | 14.6.2022 | 1.14    |

___



##### Tested OS

| OS                  | Tested |
| ------------------- | ------ |
| Ubuntu 20.04.4 LTS  | yes    |
| macOS Monterey 12.4 | yes    |
| Windows 10          | no     |
| Windows 11          | no     |





##### Usage

To navigate in the directory:

```bash
cd /dir/OS--Project
```



To run the program/execute the binary 

```bash
./teCS
```



To open a existing file:

```
./teCS test.txt
```





##### Keyboard 

Supported Keys:

| Usage       | Function                               | After Usage                |
| :---------- | -------------------------------------- | -------------------------- |
| Ctrl-s      | Save file                       | type filename and filetype |
| Ctrl-i      | Show information bar                  | -                          |
| Ctrl-f      | Search through file                    | type word or character     |
| Ctrl-q      | Quit the program                       | -                          |
| Ctrl-h      | Backspace                              | -                          |
| Del         | Delete                                 | -                          |
| Backspace   | Delete                                 | -                          |
| Enter       | Insert new line                         | -                          |
| Tab         | Tab                                   | -                          |
| Arrow Up    | Go up in file                          | -                          |
| Arrow Down  | Go down in file                        | -                          |
| Arrow Left  | Go left in file                        | -                          |
| Arrow Right | Go right in file                       | -                          |
| Space       | Insert a space after word or character | -                          |
| Pg-Up       | Jumps to the beginning of the window   | -                          |
| Pg-Down     | Jumps to the end of the window         | -                          |





##### Supported Filetypes

Supported file types: 

- Mainly for .txt filetype 
- Could open also open other filetypes e.g.  
  .png .jpg  etc. but inexpressive because of header.

