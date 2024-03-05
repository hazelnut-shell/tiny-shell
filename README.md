# Tiny Shell

## Execute built-in commands and executable files
login
![loign](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/200d8240-a6d1-4ee6-ac59-c70866899fea)

compile loop.c which does an infinite loop for future testing
![compileloop](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/8a7228d4-cb3d-4b17-a177-bdf86aea9f25)

execute the loop executable: start one foreground job and one background job
![runloop](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/935a86d8-c6d4-4aa2-b721-825577ce7845)

switch job between background and foreground
![bgfg](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/e71c068d-7a7a-44f3-8643-089c2ba12c5c)

view history list and execute history command
![history](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/ba145f54-2af4-49d6-a0b9-84c080dd9cf1)

logout and quit  
when there are suspended (stopped) jobs, user can't logout.  
the quit command kills all existing jobs and then quits.
![logoutquit](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/cf0ba92f-e915-465c-9eb4-63acbb20c35d)

## Pipes and redirection

simple redirection
![simple_redir](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/35a8fc1f-a5d6-489b-aacc-16058bdeaf7e)

multiple redirection operators
![multiple_redir1](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/7614500c-ff6a-4e38-b8a8-58c12baa739f)

use fd number in redirection operation
![multiple_redir2_usefd](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/438c144a-6a0b-43c1-be6e-1ad6a267c886)

redirection can appear anywhere in a command
![redir_random_pos](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/619d2190-0bd7-4044-a7bb-a3642cff730f)

multiple pipes
![pipe_multiple](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/ea4a5c02-abab-4e34-befa-23ed906af32e)

both built-in commands and executables can be in pipes
![pipe_builtin_plus_executable](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/d3d93fba-d3dd-4a12-aa03-a303925349e1)

pipes and redirection operators combined
![pipe_redir_combined](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/cf3000a1-f01f-4187-9d49-dc7bc20702b7)

![pipe_redir_combined2](https://github.com/hazelnut-shell/tiny-shell/assets/114367260/534f5164-3da4-4fa0-9a15-7eb650d11735)

