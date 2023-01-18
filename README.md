# HPCGame.Minesweeper

## Overview 

This repository contains code for the problem "Minesweeper"  (Chinese name: "走，我们扫雷去") in the competition Peking University High Performance Computing Game 0th (Chinese name: "第零届北京大学高性能计算综合能力竞赛").

This problem, "Minesweeper", is inspired by the game Minesweeper  under the Windows system. In our problem, The player is asked to write a program, simulating the action of a real world minesweeper player. The program can click on a certain square via a API and get the result. For more detail, please refer to the [problem statement](#problem_statement)

## Problem Statement

We provide statements in both Chinese and English. [Chinese Version](statement/zh-cn.md); [English Version](statement/en-us.md).

## About This Repository

This repository contains everything related to the minesweeper problem.

It contains:

- The judger `judger.cpp`.
- The game server `game_server.cpp`. It is responsible for interacting with the player's program.
- The standard solution `answer/expand_with_queue_mt.cpp`.
- Some naive & imperfect solutions. They are under the `answer/` diirectory.
- The data generator `map_generator.cpp`.
- The map visualizer `map_visualizer.cpp`.
- Problem statements. [Chinese Version](statement/zh-cn.md); [English Version](statement/en-us.md).
- Some other stuff, like `Makefile`.

For technical & implementation detail, please refer to comments in those source files.

## Build

Just clone this repository, and execute `make`.

You can also use `make handout` to generate `minesweeper_handout.zip`.