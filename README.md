<div id="readme-top"></div>


<!-- PROJECT LOGO 
<br />
<div align="center">
  <a href="https://github.com/github_username/repo_name">
    <img src="images/logo.png" alt="Logo" width="80" height="80">
  </a>
  -->

<div align="center">

<h2 align="center">Debugger for Patmos
</h2>

<h3 align="center">Special Course with Prof. Martin Schoeberl</h3>
<h3 align="center"></h3>

  <p align="center">
    This repository contains the code and the report produced within the scope of a <i>special course</i> oriented by <a href="https://orbit.dtu.dk/en/persons/martin-sch%C3%B6berl">Professor Martin Schoeberl</a>, as part of my Master's degree programme at Technical University of Denmark, in the academic year of 2023/2024.
  </p>
</div>

<div align="center" highlight="blue">

</div>


<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#introduction">Introduction</a>
      <ol>
        <li><a href="#this-project">This project</a>
        <li><a href="#special-course">But what is a <i>special course</i> after all?</a>
        <li><a href="#repository">Repository</a>
      </ol>
    </li>
    <li><a href="#try_it_on_your_machine">(don't) Try it on your machine</a></li>
    <li><a href="#conclusion">Conclusion & future work</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>


<!-- ABOUT THE PROJECT -->
<!-- ## About The Project -->

<h2 id="introduction">Introduction</h2>

Nowadays, in a world where technology is becoming more and more ubiquitous, the use of tools that aid development are essential and taken for granted. One such tool is called a debugger.

On a similar note, safety-critical systems – systems upon which our life can depend – have also become omnipresent. To ensure that no accidents or unexpected events happen, these systems need to be carefully analysed to make sure that their response times is within acceptable and sensible bounds. <a href="http://patmos.compute.dtu.dk/">Patmos</a> is a new processor designed with the thought of facilitating the determination of the worst-case execution time (WCET).


<h3 id="this-project">This project</h3>

Currently, Patmos can execute C and C++ code through a compiler adapted from LLVM. However, no debugging solution had been created yet for this new processor. The goal of this project was to implement a debugging solution that allows developers to inspect programs executing on the Patmos processor.


<a href="https://www.sourceware.org/gdb/">GDB</a> is a mature debugger problem with years of development and many functionalities. One interesting feature is that it allows for a program executing on a different machine than GDB to be debugged, called remote debugging. This is realised through the GDB Remote Serial Protocol (RSP). In this project, I focused on the implementation of this protocol, so that GDB could be remotely used to debug a program executing on the Patmos, implemented on a FPGA.


<h3 id="special-course">But what is a <i>special course</i> after all?</h3>
DTU encourages students to take a hands-on approach in their studies since the begining of their academic journey. Interested students can participate in research or delve into a specific topic through the development of projects with the supervision of a professor. Special courses are DTU's way of granting ECTS to students who whish to participate in such an activity. These ECTS count towards the block of elective courses.

<h3 id="repository">Repository</h3>
The repository contains 4 files:

- this README file
- the project report, a detailed documentation including: how the project was realised; what went well and not-so well; analysis of results and discussion of future work.
- the project presentation which I used to present the project to a team of researchers and other students who accompained me during the journey.
- the code developed for the course: it includes the stub functions that allows Patmos to handle communication with GDB, as well as a small program to be debugged. The program was developed in `C`.


---

If you are curious about the project, please take a look at the project report. It covers the project and all its details as much as possible. Hope you enjoy! :sunflower:

---

<p align="right">(<a href="#readme-top">back to top</a>)</p>


<!-- GETTING STARTED -->
<h2 id="try_it_on_your_machine">Try it yourself</h2>
To reproduce the results described in the report - this is, to debug the program `program.c`, the first step is to implement Patmos on an FPGA. In the project, the Altera DE2-115 FPGA was used. The <a href="http://patmos.compute.dtu.dk/patmos_handbook.pdf">Patmos Reference Handbook</a> explains how this can be done.


After Patmos is set up on the FPGA, both the target and the host sides need to be started:

<h4>The target side - execute the program on Patmos</h4>

The program to be debugged needs to be compiled and downloaded into the FPGA. First, it must be placed on the `c` directory inside the patmos development directory (in my case, `~/t-crest/patmos/`).

Then, the following command will make Patmos compile the program and start executing it:
```bash
make BOOTAPP=bootable-bootloader APP=program comp config download
``` 
After the program starts executing, the `download` step will remain active, keeping the UART connection open. Since GDB will communicate with Patmos with this same connection, this connection must be closed. This can be done by killing the program with `Ctrl+C`. The program will continue to execute on Patmos, even though the output will not be shown on the terminal.

<h4>The host side - execute GDB on the development machine</h4>

The development machine is reponsible for executing GDB. This cannot be done with the simple `gdb` command. Instead, `gdb-multiarch` needs to be used, since it is necessary to select a non-default architecture. It can be executed with:
```bash
gdb-multiarch --baud 115200 <path to ELF file>
```
The path to the ELF file should be on `~/t-crest/patmos/tmp/program.elf` or similar. GDB should then wait for the user's input. A few commands need to be executed to establish a debugging session:
- Define the architecture as `MIPS`. The reason for this is discussed in the report.
```gdb 
set architecture mips
```
- (Optional) It might be interesting to see information on what GDB is doing <i>behind-the-scenes</i>. The following command provides these insights.
```gdb
set debug remote 1
```
- Finaly, a connection can be established between the two counterparts.
```gdb
target remote /dev/ttyUSB0
```

This last command will trigger an initial exchange of messages between the host and the target, which will serve as the setup for the debugging session. The user will soon be prompted for GDB commands which will allow them to debug a program.

<p align="right">(<a href="#readme-top">back to top</a>)</p>


<!-- GENERAL CONCLUSION -->
<h2 id="conclusion">Conclusion & future work</h2>

This course resulted in the development of a solid step towards an usable debugging solution for the Patmos processor. Even though a complete debugging program was not achieved, a few key functionalities were successfully implemented, including:
- the set up of debugging sessions between the host and the target, in which they are able to correctly communicate through UART.
- on the target's side: the handling of the the received packets from the host's side, and sending information such as register values and program counter.
- the setting up of breakpoints, which are handled by the target by halting the program execution in the expected instruction.

However, to acheive a complete debugging solution for Patmos, a few aspects would need to be considered, including:
- GDB would need to be provided with a description of the Patmos architecture, instead of using MIPS.
- the stub functions should be decoupled from the program that is to be debugged.
- the Patmos compiler would need to be extended to produce executables with debugging information.


<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
<h2 id="acknowledgments">Acknowledgments</h2>

[DTU - Danmarks Tekniske Universitet](https://www.dtu.dk/)

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->

[forks-shield]: https://img.shields.io/github/forks/immarianaas/c-quiz-language.svg?style=for-the-badge
[forks-url]: https://github.com/immarianaas/c-quiz-language/network/members

[stars-shield]: https://img.shields.io/github/stars/immarianaas/c-quiz-language.svg?style=for-the-badge
[stars-url]: https://github.com/immarianaas/c-quiz-language/stargazers

[issues-shield]: https://img.shields.io/github/issues/immarianaas/c-quiz-language.svg?style=for-the-badge
[issues-url]: https://github.com/immarianaas/c-quiz-language/issues

[license-shield]: https://img.shields.io/github/license/immarianaas/c-quiz-language.svg?style=for-the-badge
[license-url]: https://github.com/immarianaas/c-quiz-language/blob/master/LICENSE





<!-- group member list -->

[linkedin-shield]: https://img.shields.io/badge/--black.svg?style=for-the-badge&logo=linkedin&colorB=0e76a8

<!-- mariana -->
[mariana-github-shield]: https://img.shields.io/badge/-Mariana-black.svg?style=for-the-badge&logo=github&colorB=555
[mariana-github-url]: https://github.com/immarianaas

<!-- arianna -->
[arianna-github-shield]: https://img.shields.io/badge/-Arianna-black.svg?style=for-the-badge&logo=github&colorB=555
[arianna-github-url]: https://github.com/AriannaBi

<!-- kajsa -->
[kajsa-github-shield]: https://img.shields.io/badge/-kajsa-black.svg?style=for-the-badge&logo=github&colorB=555
[kajsa-github-url]: https://github.com/KajsaAviaja


<!-- kári -->
[kari-github-shield]: https://img.shields.io/badge/-kári-black.svg?style=for-the-badge&logo=github&colorB=555
[kari-github-url]: https://github.com/Karisv



<!-- end membros -->



[Python-logo]: https://img.shields.io/badge/Python-306998?style=for-the-badge&amp;logo=python&amp;logoColor=white
[Python-url]: https://python.org



[antlr-shield]: https://img.shields.io/badge/ANTLR4-EF3225?style=for-the-badge
[antlr-url]: https://www.antlr.org/

[java-shield]: https://img.shields.io/badge/Java-007CBD?style=for-the-badge
[java-url]: https://www.java.com/

[example-shield]: https://img.shields.io/badge/Bootstrap-563D7C?style=for-the-badge&logo=bootstrap&logoColor=white
[example-url]: https://getbootstrap.com/
