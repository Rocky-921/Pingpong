#include <stdio.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>


#define WIDTH 80
#define HEIGHT 30
#define OFFSETX 10
#define OFFSETY 5

typedef struct {
    int x, y;
    int dx, dy;
} Ball;

typedef struct {
    int x; 
    int width;
} Paddle;

typedef struct {
    int running;
    Ball ball;
    int paddleAx;
    int scoreA, scoreB;
} GameState;    // to be sent by server, received by client

int sends = 0, recvs = 0;   // for debugging purposes

Ball ball;
Paddle paddleA, paddleB;
Paddle *my_paddle;          // client paddle or server?
int game_running = 1;
int scoreA = 0, scoreB = 0;

void init();
void end_game();
void draw(WINDOW *win);
void *move_ball(void *args);
void *com_client(void *arg);
void *com_server(void *arg);
void update_ur_paddle(int ch);
void reset_ball();
void run_game(int client_fd);

void server(int port){
    /* INITIALIZING SERVER */
    // DO ERROR HANDLING!!!
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;

    int addrlen = sizeof(server_addr), addrlen_client = sizeof(client_addr);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);         // PORT to host on?

printf("Starting bind\n");
    if (bind(server_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
printf("Bind successful\n");

    if(listen(server_fd, 1) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

printf("Listening on port %d\n", port);
    client_fd = accept(server_fd, (struct sockaddr * restrict) &client_addr, (socklen_t *)&addrlen_client);
    if (client_fd < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
printf("Connected to client\n");
    /* SERVER INITIALIZATION DONE */
    

    init();

    pthread_t ball_thread, com_thread;
    pthread_create(&ball_thread, NULL, move_ball, (void *)&client_fd);      // only server calculates the ball position
    pthread_create(&com_thread, NULL, com_server, (void *)&client_fd);      // thread for receiving data sent by client and responding it

    /*
    sender sends the game_state to client in 4 cases
    case 1: server quits the game
    case 2: server changes it's paddle position
    case 3: server ball moves, which is every 80ms
    case 4: server receives the paddle position from client

    Sender receives client's paddle position only using the communication thread
    */

    while (game_running) {
        int ch = getch();
        if (ch == 'q') {        // case 1: server quits the game
            game_running = 0;   /* if we want to exit by pressing 'q' */
            GameState game_state = (GameState){game_running, ball, paddleA.x, scoreA, scoreB};
            send(client_fd, (void *)&game_state, sizeof(game_state), 0);
            sends++;
            break;
        }
        update_ur_paddle(ch);
        if(ch != ERR){          // case 2: server changes it's paddle position
            GameState game_state = (GameState){game_running, ball, paddleA.x, scoreA, scoreB};
            send(client_fd, (void *)&game_state, sizeof(game_state), 0);
            sends++;
        }
        draw(stdscr);
    }

    pthread_join(com_thread, NULL);
    pthread_join(ball_thread, NULL);
    end_game();


    close(client_fd);
    close(server_fd);
}


void client(char *ip, int port){
    // Client code to be implemented here
    int client_fd;
    struct sockaddr_in server_addr;

    unsigned int addrlen = sizeof(server_addr);
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);                         // PORT to connect to?
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {   // IP address to connect to?
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }
printf("Connecting to server at %s\n", ip);
    if (connect(client_fd, (struct sockaddr * restrict) &server_addr, (socklen_t)addrlen) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
printf("Connected to server\n");
    

    init();
    
    pthread_t com_thread;
    pthread_create(&com_thread, NULL, com_client, (void *)&client_fd);  // thread for receiving data sent by the server

    /*
    Client sends data to server under 2 cases:-
    case 1: client quits the game
    case 2: client changes it's paddle position
    */

    while (game_running) {
        int ch = getch();
        if (ch == 'q') {        // case 1: client quits the game
            game_running = 0; /* if we want to exit by pressing 'q' */
            int tmp = -1;
            send(client_fd, (void *)&tmp, sizeof(tmp), 0);
            sends++;
            break;
        }
        if(ch != ERR){  // case 2: client changes it's paddle position
            update_ur_paddle(ch);
            send(client_fd, (void *)&paddleB.x, sizeof(paddleB.x), 0);
            sends++;
        }
        draw(stdscr);
    }

    pthread_join(com_thread, NULL);
    end_game();


    close(client_fd);
}

int main(int argc, char *argv[]) {
    if(argc < 3){
        printf("Usage:\nServer: %s server <port>\nClient: %s client <ip> <port(default: 5789)>\n", argv[0], argv[0]);
        return -1;
    }

    printf("Starting Game...\n");
    ball = (Ball){WIDTH / 2, HEIGHT / 2, 1, 1};
    paddleA = (Paddle){WIDTH / 2 - 3, 10};
    paddleB = paddleA;

    if(strcmp(argv[1], "server") == 0){     // ./pingpong server <port>
        my_paddle = &paddleA;
        server(atoi(argv[2]));
    }else{          // ./pingpong client <ip> <port>
        int port = 5789;    // default
        if(argc == 4){
             // if port is provided
            port = atoi(argv[3]);
        }
        my_paddle = &paddleB;
        client(argv[2], port);        // 127.0.0.1 -> local
    }
    return 0;
}

void init() {
    initscr();
    start_color();
    init_pair(1, COLOR_BLUE, COLOR_WHITE);
    init_pair(2, COLOR_YELLOW, COLOR_YELLOW);
    timeout(10);                    
    keypad(stdscr, TRUE);  
    curs_set(FALSE);
    noecho(); 
}

void end_game() {
    endwin();  // End curses mode
}

void draw(WINDOW *win) {

    erase();  // Clear the screen

    // Draw the border
    attron(COLOR_PAIR(1));
    for (int i = OFFSETX; i <= OFFSETX + WIDTH; i++) {
        mvprintw(OFFSETY-1, i, " ");
    }
    mvprintw(OFFSETY-1, OFFSETX + 3, "CS3205 NetPong, Ball: %d, %d", ball.x, ball.y);
    mvprintw(OFFSETY-1, OFFSETX + WIDTH-25, "Player A: %d, Player B: %d", scoreA, scoreB);
    
    mvprintw(OFFSETY-1, OFFSETX + 31, " sends: %d, recvs: %d", sends, recvs);

    for (int i = OFFSETY; i < OFFSETY + HEIGHT; i++) {
        mvprintw(i, OFFSETX, "  ");
        mvprintw(i, OFFSETX + WIDTH - 1, "  ");
    }
    for (int i = OFFSETX; i < OFFSETX + WIDTH; i++) {
        mvprintw(OFFSETY, i, " ");
        mvprintw(OFFSETY + HEIGHT - 1, i, " ");
    }
    attroff(COLOR_PAIR(1));
    
    // Draw the ball
    mvprintw(OFFSETY + ball.y, OFFSETX + ball.x, "o");

    // Draw the paddle
    attron(COLOR_PAIR(2));
    for (int i = 0; i < paddleA.width; i++) {
        mvprintw(OFFSETY + HEIGHT - 4, OFFSETX + paddleA.x + i, " ");
    }
    attroff(COLOR_PAIR(2));

    // Draw the paddle
    attron(COLOR_PAIR(2));
    for (int i = 0; i < paddleB.width; i++) {
        mvprintw(OFFSETY + 2, OFFSETX + paddleB.x + i, " ");
    }
    attroff(COLOR_PAIR(2));
    
    refresh();
}

void *move_ball(void *client_fd) {
    int fd = *(int *)client_fd;
    while (game_running) {
        // Move the ball
        ball.x += ball.dx;
        ball.y += ball.dy;

        // Ball bounces off left and right walls
        if (ball.x <= 2 || ball.x >= WIDTH - 2) {
            ball.dx = -ball.dx;
        }

        /*
        paddleA is at HEIGHT - 4
        paddleB is at 2
        
        */

        // Ball hits the paddleA
        if (ball.y == HEIGHT - 5 && ball.x >= paddleA.x -1 && ball.x < paddleA.x + paddleA.width + 1) { // Ball hits the paddleA
            ball.dy = -ball.dy;
        }else if (ball.y == 2 && ball.x >= paddleB.x -1 && ball.x < paddleB.x + paddleB.width + 1) { // Ball hits the paddleB
            ball.dy = -ball.dy;
        }

        // Ball goes past paddle down (Game Over)
        if (ball.y >= HEIGHT - 2) {
            scoreB++;
            reset_ball();
        }else if (ball.y <= 0) { // Ball goes past paddle up (Game Over)
            scoreA++;
            reset_ball();
        }

        // case 3: server ball moves, which is every 80ms
        GameState game_state = (GameState){game_running, ball, paddleA.x, scoreA, scoreB};
        send(fd, (void *)&game_state, sizeof(game_state), 0);
        sends++;
        // Slow down ball movement
        usleep(80000); 
    }
    return NULL;
}

void *com_server(void *client_fd){
    int fd = *(int *)client_fd;
    while (game_running) {
        int tmp;
        recv(fd, (void *)&tmp, sizeof(tmp), 0);     // BLOCKING CALL
        recvs++;
        if(tmp == -1){
            game_running = 0;
            break;
        }
        paddleB.x = tmp;
        // case 4: server receives the paddle position from client
        GameState game_state = (GameState){game_running, ball, paddleA.x, scoreA, scoreB};
        send(fd, (void *)&game_state, sizeof(game_state), 0);
        // Respond to client
        sends++;
    }
}

void *com_client(void  *client_fd){
    int fd = *(int *)client_fd;
    while (game_running) {
        GameState game_state;
        recv(fd, (void *)&game_state, sizeof(game_state), 0);   // BLOCKING CALL
        recvs++;
        if(game_state.running == 0){
            game_running = 0;
            break;
        }
        ball = game_state.ball;
        paddleA.x = game_state.paddleAx;
        scoreA = game_state.scoreA;
        scoreB = game_state.scoreB;
    }
}

void update_ur_paddle(int ch) {
    // Update ur paddle

    if (ch == KEY_LEFT && my_paddle->x > 2) {
        my_paddle->x--;  // Move paddle left
    }
    if (ch == KEY_RIGHT && my_paddle->x < WIDTH - my_paddle->width - 2) {
        my_paddle->x++;  // Move paddle right
    }
}

void reset_ball() {
    ball.x = OFFSETX + WIDTH / 2;
    ball.y = OFFSETY + HEIGHT / 2;
    ball.dx = 1;
    ball.dy = 1;
}
