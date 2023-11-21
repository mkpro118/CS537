# Project 6

- Names: Mrigank Kumar, Saanvi Malhotra
- CS Logins: mrigank, malhotra
- WISC IDs: 9083537424, 9083552423
- Emails: mkumar42@wisc.edu, smalhotra9@wisc.edu

## INTEGRITY STATEMENT
EXCEPT FOR THE GIVEN SOURCE CODE, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY US. NO LARGE LANGUAGE MODEL OR OTHER ONLINE OR OFFLINE SOURCE WAS USED FOR THE DEVELOPMENT OR MODIFICATION OF THE SOURCE CODE OF THIS PROJECT.

# Project Implementation and Description.

## Status
The implementation submitted on Monday, 20th November 2023, passes all tests defined in the `~cs537-1/tests/P6` directory
- `~cs537-1/tests/P6/runtests`: Scores 10/10 (Score: 10)

## List of Files Modified
- `proxyserver.c`
- `proxyserver.h`

# List of Files Added
- `safequeue.c`
- `safequeue.h`

## Implementation Description
We implemented an thread safe priority queue as a max heap to help with our implemention of this project. This priority queue has the number of elements, maximum capacity, an array for the max-heap, a mutex for thread safety, and a condition variable for blocking when the queue is empty. 

Lets walk through the functions in `safequeue.c`. First, `pq_init` allocates memory, if needed, and intializes all the members of the struct (described above). The `pq_destroy` frees all the memory allocated for the priority queue and calls the clean up function for each element in the queue. Finally, it destroys the condition variable and mutex lock. `is_pq_full` and `is_pq_empty` just check if the priority queue is full or empty. The `pq_enqueue` adds an element into the priority queue and performs a max heap insertion by percolating the newly added element up to maintain the max heap property. If the queue is full, an error message will be printed and failure will be returned. Next, the `pq_dequeue` function removes an element from the priority queue. It replaces the root with the last element and percolating the new root down to maintain the max heap property. If the priority queue is empty, we return null.

In `safequeue.h`, we create a macro for element swapping in the priority queue. We also have struct definitions for the priority queue and elements in the priority queue which is called `pq_element`. The rest is just function prototypes for the functions present in `safequeue.c`.

Next- we will walk through additions and changes to `proxyserver.c`.

### MODFICATIONS

In the `send_error_response` function, we added code to shut down the `client_fd` to close the connection. We also freed the memory allocated to the buffer.

In `serve_request` we change the parameter from an `int` to a `struct proxy_request*`. This is done to ensure we have access to extra information needed for the implementation. If delay is specified, the worker thread sleeps for the required amount of time. We modified creating a request to the target server. We now use sprintf to format a HTTP request string using a template declared earlier in the code and store it in a buffer. In case of a failure we just close the connection to the client, free all resources.

We modified the `serve_forever` function to be called by the listener threads to accept client requests and add them to the priority queue. In the implementation, an array of servers and an array of pthreads are utilized to handle connections. Each pthread needs to determine which server end to use in the server file descriptors array. To achieve this, an additional array, `thread_idx` is used. The values in the `thread_idx` array are used to index into both the servers and ports arrays. Each pthread takes the address of its corresponding index in the `thread_idx` array and passes it to the `serve_forever` function.

The `serve_forever` function takes the address of the an element in the index array as an argument, casts it to an integer pointer and dereferences it to obtain the integer index. Using this value, the function retrieves the address of the `server_fd` from the index and replaces it with the socket's file descriptor obtained from the socket function. The threads, both workers and listeners, wait for an exit flag to be set before joining. Once all threads have completed, the program proceeds to free proxy request variables, close client file descriptors, destroy the priority queue, and release all allocated resources, ensuring a clean shutdown of the server.

In the function, there's a server loop that continuously accepts connections as long as the exit flag is not set. When a connection is accepted, it proceeds to parse the HTTP request headers from the client. If parsing fails, an error message is printed, and the loop continues to the next iteration. If the parsed request is identified as a GetJob request (by checking if the path matches the predefined `GETJOBCMD` macro), the server attempts to retrieve a job from the queue without blocking. If there's no job in the queue, it sends an error response to the client indicating that the queue is empty. On the other hand, if a job is obtained from the queue, it sends a response with the job's path, destroys the associated HTTP request structure, frees all resources. For file requests, a proxy request structure (`struct proxy_request`) is created and initialized with information from the parsed HTTP request. This proxy request, along with a priority queue element (`pq_element`), is then prepared for addition to the priority queue. If the addition is successful, the server moves on to the next iteration of the loop. However, if the queue is full, it sends an error response to the client, cleans up allocated resources, and proceeds to the next iteration.

The `signal_callback_handler` function serves as a handler for signals, particularly designed for handling the SIGINT signal triggered by `Ctrl+C`. Upon catching a signal, it prints a message detailing the signal number and its description. Crucially, it sets the `EXIT_FLAG` to 1, indicating that an exit condition has been triggered, and broadcasts to all threads waiting on the `pq_cond_fill` condition variable. This broadcast signals to all threads that they should prepare for a graceful exit, facilitating a clean shutdown of the program. In the main part of the program, a setup for the signal handler is implemented using the `signal` function, configuring it to utilize the `signal_callback_handler` function for handling SIGINT signals. This adjustment enables us to manage an orderly exit when prompted by the user through a Ctrl+C command.

We malloc our `server_fds` array as described above within main once we know how many listeners there are and we also malloc the listener threads and create and initialize the priority queue. We malloc and initialize our `thread_idx` array and then create our listener threads which will run the serve forever function until they get a signal interrupt. We pass in the address of the `thread_idx` array element for that thread to the `serve_forever` function to use. We make our worker threads next that call the `do_work` function. Both the listener and worker threads wait to exit on a SIGINT and we do so by running `pthread_join`. Once the threads have exited, we clean up all the resources we used and shutdown the server thus returning success!


### ADDITIONS:

We added a function called `parse_priority` that takes a path as input and extracts a numeric priority value from the path. The parsing functions by reading numeric characters started after the first forward slash until we hit the next forward slash.

Next, we added a function called `do_work` that takes in a void pointer (as required by `pthread_create`). It is used by the worker thread to continously call get_work until there is a request in the queue at which point it serves the request. It returns when the `EXIT_FLAG` is set

The `http_request_cleanup` function takes a single argument, a pointer to a `struct http_request`, and invokes the `http_request_destroy` function to properly clean up resources associated with HTTP requests. This function is used to ensure the correct destruction of dynamically allocated memory and other resources tied to HTTP requests within the context of a multi-threaded environment. Its purpose is to maintain resource integrity and prevent memory leaks or other issues related to the allocation and deallocation of HTTP request structures.

Finally - we walk though the changes to `proxyserver.h`. We simply declare the function headers and our structs for an `http_request` and `proxy_request`. In addition, we modify `http_request_parse` to parse out how much time - if any we should delay for. We also create a `http_request_destroy` function to get rid of a `http_request` struct gracefully and free all used resources.
