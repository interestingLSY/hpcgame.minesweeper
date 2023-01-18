## Background

You like playing the game "Minesweeper" very much.

In the game "Minesweeper", you are facing a matrix containing $N \times N$ squares, and each square may have a mine or no mine.

Your basic operation is called "click". Every time, you can choose a square and "click" it, if it is a mine, then your points will be deducted, if it is not a mine, then you will get one point, and a number will show up on the square, indicating how many mines (0~8) are there in the surrounding eight squares (up, down, left, right, upper left, lower left, upper right, lower right). If the number in the clicked square is 0 (this means that all the eight squares around this square are not mines), then the surrounding squares will also be automatically "clicked" (this process can be recursive, that is, if the number in the new square is also 0, then the eight surrounding squares will also be automatically "clicked". We call it "indirect click", corresponding to the square directly clicked by the player, called "direct click"). When the player voluntarily quits, or the time runs out, the game ends.

Please note that there are some differences between the minesweeper game for this question and the standard minesweeper game in the Windows system:

- In the standard minesweeper game, clicking on a square containing mines will cause the game to fail immediately, while in our game, your score will be deduced, but the game will continue.
- When all non-mine squares are clicked, the game will not end automatically, and the player's program needs to actively exit.
- Every time you "click" a square containing number 0, your program will receive all the squares with the number 0 in the connected block containing the clicked square and the squares with numbers other than 0 at the edge, even if these squares have already been clicked before. This is described in detail later.

Here is an example of the "Minesweeper" game on the KDE desktop system (a small red flag means that there are mines in the square, and an empty square means that the number in the square is 0):

![Quoted from https://commons.wikimedia.org/wiki/File:Minesweeper_end_Kmines.png](https://hpcgame.pku.edu.cn/oss/images/Minesweeper_end_Kmines.png)


After practicing day and night, the $30 \times 16$ map size of the advanced version of Minesweeper is already a piece of cake for you. So, you decide to play something more exciting. What about, minesweeper for $65536 \times 65535$?

Of course, minesweeping on such a large scale cannot be done by hand. So, you plan to integrate what you have learned all your life into a program, and let this program solve the game.

## Description

You need to write a (possibly parallel) program to interact with the minesweeper game server we provide.

You can use the following library functions to interact with the game server:

- `void minesweeper_init(int &N, int &K, int &constant_A);` Before the program starts, please call this function to initialize the program. This function will store the side length $N$ of the map in the variable `N`, the number of mines $K$ in the map in the variable `K`, and the scoring parameter $A$ (see below) in the variable `constant_A`.

- `Channel create_channel(void);` Please use this function to create a `Channel` object for communicating with the game server. Ensure that different `Channel` do not interfere with each other, but if two processes/threads operate the same `Channel` at the same time, you will be blown up. So it is recommended to open a `Channel` for each process/thread separately.

   You can create up to 1024 Channels with this function, but I recommend that you try to keep no more than 8 Channels active (calling `click()` repeatedly). After all, for each Channel you create, a separate thread in the game server is responsible for handling requests in that Channel. We only provide 16 cores during evaluation (note: there is no hyperthreading), so if there are more than 8 active channels, frequent context switches will occur, which greatly affects performance.

- `ClickResult Channel::click(int r, int c, bool skip_when_reopen);` This is a member function of `Channel`, which represents the operation of "clicking on the square corresponding to the position of $(r, c)$", $0 \le r, c < n$.

   See `minesweeper_helpers.h` for the definition of `ClickResult` class and its member variables.

   Every time you "click" a square containing the number 0, your program will receive all the squares with the number 0 in the connected block where the clicked square is located and the squares with numbers other than 0 at the edge, even if these squares are already been clicked. For example: Suppose the map is as follows (X represents mines, the coordinates of the upper left corner are $(0, 0)$, the coordinates of the upper right corner are $(0, 2)$, and the coordinates of the lower right corner are $(2, 2)$):

   ```
   0 1 X
   1 2 1
   X 1 0
   ```

   Then when calling `click(0, 0, false)`, the clicked square contains $(0, 0), (0, 1), (1, 0), (1, 1)$; then if you call `click(2, 2, false)`, then the clicked square contains $(1, 1), (1, 2), (2, 1), (2, 2)$. Please note that in both calls, the $(1, 1)$ box is clicked. Corresponding to the program, that is $(1, 1)$ This point appears in the `open_grid_pos` array of `ClickResult` returned by `click()` twice.

   If `skip_when_open` is `true`, then if the square $(r, c)$ has been clicked before (whether it is opened by this `Channel` or other `Channel`, and whether directly clicked or indirectly clicked), then the `click` function will immediately return a `ClickResult` with `is_skipped=true` (Note: The main purpose of providing this operation is to save time). Please note that this option guarantees that if `is_skipped=true` is returned, then the square $(r, c)$ must have been clicked before; however, if two threads click on the same square approximately at the same time (or both indirectly click on the opponent's square), then there is a certain probability that neither of them will skip. In other words, there are some data race issues here.

   The complexity of this function is: $\Theta({\small\text{The number of squares opened}})$. If `skip_when_open=true` and the square was opened before, or the square contains mines, then the complexity of the function is $\Theta(1)$.

- `ClickResult Channel::click_do_not_expand(int r, int c);` This is a member function of `Channel`, which means "click to open the square corresponding to the position of $(r, c)$, and do not click indirectly".

   Unlike the `click()` function above, this function does not do indirect clicks. In other words, assuming that the number in the square $(r, c)$ is $0$, and the numbers in the surrounding squares are not $0$, then `click(r, c)` will click and return $(r, c)$ and its surrounding squares, while `click_do_not_expand(r, c)` will only click and return the square $(r, c)$.

   The complexity of this function is always $\Theta(1)$.

   (Note: The author of the question found after writing the standard solution that this function seems to be more commonly used than the `click()` above...)

These functions are defined in `minesweeper_helpers.h`. You can add `#include "minesweeper_helpers.h"` at the beginning of your program to use these functions.

## Example Solution

For your programming convenience, we provide sample programs `naive.cpp` and `interact.cpp`. `naive.cpp` will use a single thread to open each square in the $N \times N$ square matrix in turn. `interact.cpp` is an interactive minesweeper client. After startup, you can enter the coordinates of the square you want to open, which allows you to play the game by your own hands.

## How to Prepare, Test and Submit

Due to the use of `futex`, this question only supports linux systems. It is recommended that students who do not have a linux system use the code server (or virtual machine or docker) provided by the contest organizer to run and debug the program.

The preparation steps are as follows:

- (if you don't use code server) install necessary packages, including make, gcc, g++, zip
- Download the additional file `minesweeper_handout.zip`, unzip it, and switch to the unzipped folder
- Execute `make`
- Execute `python3 generate_example_maps.py`. This program will call `map_generator` to generate map files with side lengths ranging from 16 to 65536 and store them in the `map/` directory. The naming rule of the map file is `N_K_seed.map`, where `N` represents the side length of the map, `K` represents the number of mines in the map, and `seed` represents the random number seed. Both the generated map file and the map file in the final test satisfy $K = N^2/8$. Please note that the random number seed of the map file at the time of final evaluation is not 0.

You can then start editing `answer.cpp`. (Note: You can use `#include "lib/csapp.h"` in `answer.cpp` to use `csapp.h` file. The corresponding `csapp.o` will be added automatically when linking)

How to test: `make && ./judger answer <map file, such as map/16_32_0.map> [time limit in seconds]`

How to submit: Submit `answer.cpp` to the website.

By the way, `naive.cpp` can pass the first 8 points.

## Grading

There are several test points, and the score of each test point is:

$\frac{\text{Number of (different) non-mine squares opened by the player within the time limit} - A \cdot \text{(Number of (different) mines opened by the player} - K\cdot0.0002)}{(N^2 -K)\times 0.9998} \times 100$, (max for 0, min for 100), where $N$ is the side length of the map, $K$ is the total number of mines in the map and $A$ is a constant. See "Test Point Scheme" below for details.

In other words: Even if you forgot to click $0.02\%$ of non-mine squares, and you accidentally clicked $0.02\%$ of mines, you can still get full marks in the end. (Be brave! It's easy!)

The total score of the problem is the sum of the scores of all test points.

## Hardware Configuration

Your program will run on our compute nodes together with `game_server` and `judger`. The configuration of the compute nodes is:

- CPU: [Intel® Xeon® Platinum 8358 Processor](https://www.intel.com/content/www/us/en/products/sku/212282/intel-xeon-platinum-8358-processor-48m-cache-2-60-ghz/specifications.html). We will provide 16 cores when your program is running, but please note that for each Channel you create, a separate thread in the game server is responsible for processing requests in that Channel. If there are more than 8 active Channels, frequent context switches will occur, which will greatly affect performance. So we recommend that the number of **active channels should not exceed eight**.
- Memory: 64GB. `game_server` and `judger` will take about 5 GB of memory together, the operating system will take up about 1 GB of memory, and use the rest as you like.

## Test Point Description

| Test Point ID | Score | N | K | Constant A | Time Limit (s) |
| --------- | ------------------ | ------------- | ------------- | ------ | ------------- |
| 0         | 8                  | 512           | 512*512/8     | 0      | 2             |
| 1         | 16                 | 16384         | 16384*16384/8 | 0      | 18            |
| 2         | 14                 | 512           | 512*512/8     | 8      | 2             |
| 3         | 16                 | 2048          | 2048*2048/8   | 8      | 8             |
| 4         | 30                 | 8192          | 8192*8192/8   | 8      | 32            |
| 5         | 30                 | 16384         | 16384*16384/8 | 8      | 20            |
| 6         | 32                 | 32768         | 32768*32768/8 | 8      | 68            |
| 7         | 44                 | 65536         | 65536*65536/8 | 8      | 256           |

If you get full score in all test points, you will be awarded with 10 points. The full score is 200.