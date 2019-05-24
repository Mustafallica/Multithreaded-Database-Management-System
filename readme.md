## The multithreaded Database Management System.
#### 
##### This application is part of COSC 3360 - Operating System Fundamentals class at the University of Houston. This application uses of pthreads, pthread mutex semaphores, and pthread condition variables.

- - - -

### Specs ###
###### A multithreaded database management system receives requests from multiple users to access a database with ten data records. The DBMS must use concurrency mechanisms to guarantee consistency when multiples requests try to access the same data record at the same time. One of the features of the DBMS is an accounting module that prints all the information regarding how the requests from the clients are being processed by the DBMS. The goal of the assignment is to simulate the operation of the DBMS using POSIX threads, POSIX semaphores, and condition variables.

1. The main thread: it creates the database and the request threads according to the input specifications.
2. One request thread per line in the input file (without including the first line): it simulates the request from a user to access a particular position of the database.
