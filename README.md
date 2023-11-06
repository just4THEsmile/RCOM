# RCOM

# Intructions to run the 1st project

1. Go to code and open a terminal and Run the cable with the makefile target
   
  ```
  $make run_cable
  ```
  
2. Run the receiver in another diferent terminal (if you run tranceiver alone and wait 3 second it will timout)
   
```
$make run_rx
 ```

4. Run the transceiver with the makefile target in another diferent terminal
```
$make run_tx
```

3. After it runs check if the file transfered is the same

```
$diff -s penguin.gif penguin-received.gif
```




   
