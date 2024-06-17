#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ACCOUNTS_FILE "taikhoan.txt"
#define PRODUCTS_FILE "products.txt"
#define TRANSACTIONS_FILE "transactions.txt"
#define LOG_FILE "log.txt"
#define MONITOR_FILE "theodoi.txt"
#define HISTORY_FILE "history.txt"

// Định nghĩa cấu trúc tài khoản
typedef struct {
    char username[50];
    char password[50];
    char id[50];       // Căn cước công dân
    long long balance; // Số dư tài khoản
} Account;

// Định nghĩa cấu trúc LoginData
typedef struct {
    GtkWidget *username_entry;
    GtkWidget *password_entry;
} LoginData;

// Định nghĩa cấu trúc Order - Dùng cho việc 
// hiển thị những mặt hàng được chọn trong màn hình đặt đồ ăn
typedef struct Order {
    char name[50];
    int quantity;
    int price;
    GtkWidget *quantity_widget;
    GtkWidget *all;
} Order;
Order *cart = NULL;
int cart_index = 0;
GtkWidget *summary_box;
GtkWidget *bill_address;
GtkListStore *store_address;
int bill_cost = 0;
int bill_total = 0;
GtkWidget *bill_total_address;

// Định nghĩa hàm Item
// Thông tin về mặt hàng trong kho
typedef struct Item {
    char name[20];
    int quantity;
    long long price;
    char dayIn[20], Exp[20];
    char imagePath[100];
} Item;
int number = 0;
Item *item = NULL;

// Định nghĩa OrderBill
// Thông tin về những mặt hàng đã được đặt
typedef struct OrderBill{
    char name[50];
    int quantity;
    int price;
    char local_time[100];
    char status[50];
} OrderBill;
OrderBill *ordered = NULL;
int ordered_index = 0;
GtkWidget *ordered_box;
GtkListStore *ordered_address;
GtkWidget *overlay;


// Định nghĩa QueueOrder
// Thông tin về những đơn hàng được gửi về admin chờ xác nhận
typedef struct QueueOrder{
    char name[50];
    int quantity;
    int price;
    char local_time[100];
    char status[50];
} QueueOrder;
QueueOrder *queue = NULL;
int queue_index = 0;
GtkListStore *queue_address;
long last_position = 0;
int id[100];

typedef struct ConfirmedFoodOrder{
    int id;
    char name[50];
    int quantity;
    int price;
    char local_time[100];
    char cf_time[100];
    char status[50];
} ConfirmedFoodOrder;
ConfirmedFoodOrder *cf_food = NULL;
GtkListStore *cf_food_address;

char current_username[50];
char password_user[50];

// Prototype các hàm
static gboolean update_user_window(gpointer data);
void calculate_time_remaining(gint *hours, gint *minutes, gint *seconds, gdouble balance, gdouble rate);
void calculate_time_use(gint *h, gint *m, gint *s, gint hours, gint min, gint sec);
static gboolean on_user_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);

// Cấu trúc dữ liệu để lưu trữ thông tin người dùng
typedef struct {
    GtkWidget *time_label;
    GtkWidget *balance_label;
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
    GtkWidget *entry_usage_time;
    GtkWidget *usage_amount;
    GtkWidget *entry_total;
    gdouble balance;
    gdouble initial_balance; // Số dư ban đầu
    gdouble rate;
    guint timeout_id;
    Account *user_account;
    GList *accounts_list;
    gboolean locked; // Thêm trường này để kiểm tra trạng thái khóa
    gchar date_str[11]; // Chuỗi để lưu ngày tháng (định dạng DD-MM-YYYY)
    gchar time_str[9];  // Chuỗi để lưu giờ phút giây (định dạng HH:MM:SS)
} UserData;

// Cấu trúc dữ liệu để lưu trữ thông tin chuyển tiền
typedef struct {
    GtkWidget *transfer_account_entry;
    GtkWidget *password_entry;
    GtkWidget *amount_entry;
    UserData *user_data; // Thêm con trỏ tới UserData
} TransferData;

static GtkWidget *login_window;
static GtkWidget *admin_window; 
static GtkWidget *user_window;

static GtkWidget *username_entry;
static GtkWidget *password_entry;

static GtkWidget* create_machines_page();
static GtkWidget* create_transactions_page();
static GtkWidget* create_orders_page();
static GtkWidget* create_accounts_page();

static GtkWidget *global_grid = NULL;
static GFileMonitor *file_monitor = NULL;

static gboolean update_grid();

// Biến số máy
static int so_may = 69; // Bạn có thể thay đổi giá trị này để kiểm tra cho các số máy khác nhau

// Hàm đọc tài khoản từ tệp
GList* read_accounts_from_file(const char *filename) {
    GList *list = NULL;
    FILE *file = fopen(filename, "r");
    if (file) {
        Account account;
        while (fscanf(file, "%s %s %s %lld", account.username, account.password, account.id, &account.balance) != EOF) {
            Account *new_account = g_slice_new(Account);
            *new_account = account;
            list = g_list_append(list, new_account);
        }
        fclose(file);
    }
    return list;
}

// Hàm ghi tài khoản vào tệp
void write_accounts_to_file(const char *filename, GList *accounts) {
    FILE *file = fopen(filename, "w");
    if (file) {
        for (GList *l = accounts; l != NULL; l = l->next) {
            Account *account = (Account *)l->data;
            fprintf(file, "%s %s %s %lld\n", account->username, account->password, account->id, account->balance);

        }
        fclose(file);
    }
}

// Hàm cập nhật file "theodoi.txt"
void update_monitor_file(int machine_number, const char *username, const char *id, const char *status, gdouble balance, gdouble rate ,gchar *day, gchar *time ) {
    FILE *file = fopen(MONITOR_FILE, "r+");
    if (!file) {
        file = fopen(MONITOR_FILE, "w");
        for (int i = 1; i <= 70; i++) {
            fprintf(file, "%d x x off 0 x x\n", i);
        }
        fclose(file);
        file = fopen(MONITOR_FILE, "r+");
    }

    if (file) {
        // Ensure the buffer size can hold the maximum line length, including newline and null terminator
        char lines[71][100];
        for (int i = 0; i < 70; i++) {
            if (fgets(lines[i], sizeof(lines[i]), file) == NULL) {
                // Ensure we handle unexpected EOF
                strcpy(lines[i], "\n");
            }
        }
        
        rewind(file);
        
        // Update the line corresponding to the machine_number
        if (strcmp(status, "on") == 0 || strcmp(status, "pause") == 0) {
            snprintf(lines[machine_number - 1], sizeof(lines[machine_number - 1]), "%d %s %s %s %.0f %s %s\n", machine_number, username, id, status, balance, day, time);
        } else {
            snprintf(lines[machine_number - 1], sizeof(lines[machine_number - 1]), "%d x x %s 0 x x\n", machine_number, status);
        }

        // Write back all the lines to the file
        for (int i = 0; i < 70; i++) {
            fprintf(file, "%s", lines[i]);
        }
        fclose(file);
    }
}


void getData() {
    FILE *f = fopen("products.txt", "r");
    if(f == NULL) {
        printf("Loi truy cap du lieu!\n");
        return;
    }
    item = (Item *)malloc(0 * sizeof(Item));
    if(item == NULL) {
        printf("Loi cap phat bo nho!\n");
        fclose(f);
        return;
    }
    char line[256];
    int i = 0;
    while(fgets(line, sizeof(line), f)){
        number++;
        item = (Item *)realloc(item, number * sizeof(Item));
        line[strlen(line) - 1] = '\0';
        strcpy(item[i].name, line);
        fgets(line, sizeof(line), f);
        sscanf(line, "%d %lld %s %s",  
                &item[i].quantity, 
                &item[i].price, 
                item[i].dayIn, 
                item[i].Exp);
        sprintf(item[i].imagePath, "C:\\Users\\Admin\\Desktop\\BT_9\\file\\pictures\\%s.jfif", item[i].name);
        i++;
    }
    fclose(f);
}

// Hàm tính toán thời gian còn lại
void calculate_time_remaining(gint *hours, gint *minutes, gint *seconds, gdouble balance, gdouble rate) {
    gdouble total_seconds = balance / rate * 3600;
    *hours = (gint)(total_seconds / 3600);
    *minutes = (gint)((total_seconds - (*hours * 3600)) / 60);
    *seconds = (gint)(total_seconds - (*hours * 3600) - (*minutes * 60));
}

// Hàm xử lý sự kiện khi người dùng cố gắng đóng cửa sổ
static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widget),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_YES_NO,
                                               "Bạn có muốn đăng xuất?");
    gtk_window_set_title(GTK_WINDOW(dialog), "Xác nhận đăng xuất");

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        if (data != NULL) {
            UserData *user_data = (UserData *)data;
            if (user_data->timeout_id > 0) {
                g_source_remove(user_data->timeout_id);
                user_data->timeout_id = 0;
            }
            g_slice_free(UserData, user_data);
        }

        gtk_widget_hide(widget); // Ẩn cửa sổ hiện tại thay vì phá hủy
        gtk_widget_show_all(login_window); // Hiển thị cửa sổ đăng nhập
        return TRUE; // Trả về TRUE để ngăn chặn cửa sổ bị phá hủy
    }
    return TRUE; // Trả về TRUE để ngăn chặn cửa sổ bị phá hủy
}

// Hàm xử lý sự kiện khi nhấn nút đăng xuất
static void on_logout_button_clicked(GtkWidget *widget, gpointer data) {
    UserData *user_data = (UserData *)data;

    // Cập nhật số dư tài khoản hiện tại
    user_data->user_account->balance = (long long)user_data->balance;

    // Cập nhật trạng thái "off" cho máy
    update_monitor_file(so_may, "x", "x", "off", 0, user_data->rate,"x","x");

    // Ghi thông tin tài khoản vào tệp
    write_accounts_to_file("taikhoan.txt", user_data->accounts_list);

    // Ngắt timeout nếu có
    if (user_data->timeout_id > 0) {
        g_source_remove(user_data->timeout_id);
        user_data->timeout_id = 0;
    }

    // Giải phóng bộ nhớ
    g_list_free_full(user_data->accounts_list, (GDestroyNotify)g_slice_free1);
    g_slice_free(UserData, user_data);

    // Đóng cửa sổ user và hiển thị cửa sổ login
    gtk_widget_destroy(user_window);
    gtk_widget_show_all(login_window);
}

// Hàm tạo cửa sổ admin
// Các hàm hỗ trợ đọc/ghi từ/đến tệp

void update_history_file(int so_may, const char *username, int type, long long balance, char do_an[]) {
    GDateTime *now = g_date_time_new_now_local();
    char date[11];
    char time[9];
    
    // Lấy thời gian hiện tại
    int year = g_date_time_get_year(now);
    int month = g_date_time_get_month(now);
    int day = g_date_time_get_day_of_month(now);
    int hour = g_date_time_get_hour(now);
    int minute = g_date_time_get_minute(now);
    int second = g_date_time_get_second(now);

    // Định dạng ngày tháng vào chuỗi
    g_snprintf(date, sizeof(date), "%02d/%02d/%04d", day, month, year);

    // Định dạng giờ phút giây vào chuỗi
    g_snprintf(time, sizeof(time), "%02d:%02d:%02d", hour, minute, second);

    g_date_time_unref(now);

    // Mở file để ghi, sử dụng mode "r+" để ghi vào đầu file
    FILE *file = fopen("history.txt", "r+");
    if (file) {
        // Đọc tất cả dữ liệu hiện tại của file vào buffer
        char buffer[10000]; // Độ dài buffer có thể tùy chỉnh
        size_t file_size = fread(buffer, 1, sizeof(buffer), file);

        // Đặt con trỏ file về đầu
        rewind(file);

        // Ghi dữ liệu mới vào đầu file
        fprintf(file, "%d %s %d %s %lld \n%s\n%s\n",so_may, username, type, time, balance, do_an, date);

        // Ghi lại dữ liệu cũ sau dữ liệu mới
        fwrite(buffer, 1, file_size, file);

        // Đóng file
        fclose(file);
    } else {
        printf("Không thể mở file history.txt để ghi dữ liệu.\n");
    }
}

void add_account(const char *username, const char *password, const char *id, long long balance) {
    // Open the file in read mode to check for existing account
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (file == NULL) {
        perror("Không thể mở tệp tài khoản");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        Account account;
        sscanf(line, "%s %s %s %lld", account.username, account.password, account.id, &account.balance);
        if (strcmp(account.username, username) == 0) {
            printf("Tên tài khoản đã tồn tại\n");
            fclose(file);
            return;
        }
    }
    fclose(file);

    // Open the file in append mode to add new account
    file = fopen(ACCOUNTS_FILE, "a");
    if (file == NULL) {
        perror("Không thể mở tệp tài khoản");
        return;
    }
    fprintf(file, "%s %s %s %lld\n", username, password, id, balance);
    fclose(file);

    // Log the account creation
    update_history_file(so_may,username,1,balance,"nothing");
}

void add_funds_to_account(const char *username, long long amount) {
    FILE *file = fopen(ACCOUNTS_FILE, "r+");
    if (file == NULL) {
        perror("Không thể mở tệp tài khoản");
        return;
    }

    char line[256];
    char new_lines[1024] = "";
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        Account account;
        sscanf(line, "%s %s %s %lld", account.username, account.password, account.id, &account.balance);
        if (strcmp(account.username, username) == 0) {
            account.balance += amount;
            found = 1;
        }
        char new_line[256];
        sprintf(new_line, "%s %s %s %lld\n", account.username, account.password, account.id, account.balance);
        strcat(new_lines, new_line);
    }

    fclose(file);

    if (!found) {
        printf("Không tìm thấy tài khoản\n");
        return;
    }

    file = fopen(ACCOUNTS_FILE, "w");
    if (file == NULL) {
        perror("Không thể mở tệp tài khoản");
        return;
    }
    fputs(new_lines, file);
    fclose(file);

    update_history_file(so_may,username,1,amount,"nothing");
    
}

void read_accounts(GtkListStore *store) {
    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (file == NULL) {
        perror("Không thể mở tệp tài khoản");
        return;
    }

    gtk_list_store_clear(store);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        GtkTreeIter iter;
        Account account;
        sscanf(line, "%s %s %s %lld", account.username, account.password, account.id, &account.balance);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, account.username, 1, account.id, 2, account.balance, -1);
    }

    fclose(file);
}

static void on_add_account_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Thêm tài khoản",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        ("Thêm"), GTK_RESPONSE_ACCEPT,
        ("Hủy"), GTK_RESPONSE_REJECT,
        NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    GtkWidget *label_username = gtk_label_new("Tên:");
    GtkWidget *entry_username = gtk_entry_new();
    GtkWidget *label_password = gtk_label_new("Mật khẩu:");
    GtkWidget *entry_password = gtk_entry_new();
    GtkWidget *label_id = gtk_label_new("Căn cước công dân:");
    GtkWidget *entry_id = gtk_entry_new();
    GtkWidget *label_balance = gtk_label_new("Số tiền:");
    GtkWidget *entry_balance = gtk_entry_new();

    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_password, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_password, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_id, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_id, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_balance, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_balance, 1, 3, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username));
        const char *password = gtk_entry_get_text(GTK_ENTRY(entry_password));
        const char *id = gtk_entry_get_text(GTK_ENTRY(entry_id));
        const char *balance_str = gtk_entry_get_text(GTK_ENTRY(entry_balance));
        long long balance = atoll(balance_str);
        add_account(username, password, id, balance);

        // Cập nhật giao diện
        GtkListStore *store = GTK_LIST_STORE(data);
        read_accounts(store);
    }

    gtk_widget_destroy(dialog);
}

static GtkWidget* create_machines_page() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Tạo cửa sổ cuộn để quản lý bố cục tốt hơn
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 0);

    // Tạo viewport để chứa lưới bên trong cửa sổ cuộn
    GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), viewport);

    // Tạo lưới và gán nó vào biến toàn cục
    global_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(global_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(global_grid), 5);
    gtk_container_add(GTK_CONTAINER(viewport), global_grid);

    // Tạo và thêm tiêu đề cột
    GtkWidget *label;
    label = gtk_label_new("Số máy");
    gtk_grid_attach(GTK_GRID(global_grid), label, 0, 0, 1, 1);
    label = gtk_label_new("Tài khoản");
    gtk_grid_attach(GTK_GRID(global_grid), label, 1, 0, 1, 1);
    label = gtk_label_new("Số dư");
    gtk_grid_attach(GTK_GRID(global_grid), label, 2, 0, 1, 1);
    label = gtk_label_new("Thời gian còn lại");
    gtk_grid_attach(GTK_GRID(global_grid), label, 3, 0, 1, 1);
    label = gtk_label_new("Tình trạng máy");
    gtk_grid_attach(GTK_GRID(global_grid), label, 4, 0, 1, 1);
    label =  gtk_label_new("Thời gian đăng nhập");
    gtk_grid_attach(GTK_GRID(global_grid),label, 5, 0 , 1 ,1 );
    label =  gtk_label_new("Ngày đăng nhập");
    gtk_grid_attach(GTK_GRID(global_grid),label, 6, 0 , 1 ,1 );

    // Cập nhật lưới với dữ liệu ban đầu
    update_grid();

    // Thiết lập giám sát tệp
    GFile *file = g_file_new_for_path("theodoi.txt");
    file_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, NULL);
    g_signal_connect(file_monitor, "changed", G_CALLBACK(update_grid), NULL);
    g_object_unref(file);

    // Cập nhật định kỳ
    g_timeout_add_seconds(1, (GSourceFunc)update_grid, NULL); // Cập nhật mỗi 10 giây

    gtk_widget_show_all(box);
    return box;
}


 static gboolean update_grid() {
    // Xóa nội dung lưới hiện có, trừ tiêu đề
    GList *children = gtk_container_get_children(GTK_CONTAINER(global_grid));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *widget = GTK_WIDGET(iter->data);
        gint top_attach, left_attach;
        gtk_container_child_get(GTK_CONTAINER(global_grid), widget, "top-attach", &top_attach, "left-attach", &left_attach, NULL);

        // Chỉ xóa các hàng có top_attach > 0, tức là không phải hàng tiêu đề
        if (top_attach > 0) {
            gtk_widget_destroy(widget);
        }
    }
    g_list_free(children);

    // Mở tệp theodoi.txt và đọc thông tin máy
    FILE *file = fopen("theodoi.txt", "r");
    if (file) {
        char line[256];
        int row = 1;
        while (fgets(line, sizeof(line), file)) {
            int machine_number;
            char username[50], id[50], status[10], day[11], time[9]; // Tăng kích thước của status để đảm bảo chứa được các giá trị
            long long balance;
            if (sscanf(line, "%d %s %s %s %lld %s %s", &machine_number, username, id, status, &balance, day , time) == 7) {
                // Trim leading/trailing whitespace from status
                char *trimmed_status = g_strstrip(status);
                
                if (strcmp(trimmed_status, "on") == 0 || strcmp(trimmed_status, "pause") == 0) {
                    gint rate;
                    if (machine_number > 0 && machine_number < 21) {
                        rate = 8000;
                    } else if (machine_number > 20 && machine_number < 41) {
                        rate = 10000;
                    } else if (machine_number > 40 && machine_number < 61) {
                        rate = 15000;
                    } else if (machine_number > 60 && machine_number < 71) {
                        rate = 20000;
                    } else {
                        rate = 0; // Handle invalid machine number
                    }

                    if (rate > 0) {
                        gint hours, minutes, seconds;
                        calculate_time_remaining(&hours, &minutes, &seconds, (gdouble)balance, rate);

                        GtkWidget *label;
                        char buffer[100];

                        // Cột 1: Số máy
                        snprintf(buffer, sizeof(buffer), "%d", machine_number);
                        label = gtk_label_new(buffer);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 0, row, 1, 1);

                        // Cột 2: Tên tài khoản
                        label = gtk_label_new(username);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 1, row, 1, 1);

                        // Cột 3: Số dư
                        snprintf(buffer, sizeof(buffer), "%lld", balance);
                        label = gtk_label_new(buffer);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 2, row, 1, 1);

                        // Cột 4: Thời gian còn lại
                        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, seconds);
                        label = gtk_label_new(buffer);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 3, row, 1, 1);

                        // Cột 5: Tình trạng máy
                        if (strcmp(trimmed_status, "on") == 0) {
                            label = gtk_label_new("Đang hoạt động");
                        } else if (strcmp(trimmed_status, "pause") == 0) {
                            label = gtk_label_new("Đang khóa máy");
                        }
                        gtk_grid_attach(GTK_GRID(global_grid), label, 4, row, 1, 1);

                        //cột 6: Thời gian đăng nhập
                        snprintf(buffer, sizeof(buffer), "%s", time);
                        label = gtk_label_new(buffer);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 5, row, 1, 1);

                        //cột 7: Ngày đăng nhập
                        snprintf(buffer, sizeof(buffer), "%s", day);
                        label = gtk_label_new(buffer);
                        gtk_grid_attach(GTK_GRID(global_grid), label, 6, row, 1, 1);

                        row++;
                    }
                }
            }
        }
        fclose(file);
    }
    gtk_widget_show_all(global_grid);
    return TRUE; // Tiếp tục gọi hàm này trong hẹn giờ
}

GtkWidget *transactions_page_vbox;
GtkWidget *deposit_page_vbox;
GtkWidget *food_page_vbox;
GtkWidget *revenue_page_vbox;

static time_t last_mod_time = 0;

// Hàm lấy thời gian sửa đổi cuối cùng của tệp
time_t get_file_mod_time(const char *filename) {
    struct stat attrib;
    if (stat(filename, &attrib) == -1) {
        perror("stat");
        return 0;
    }
    return attrib.st_mtime;
}

// Hàm cập nhật trang giao dịch
void populate_transactions_page(GtkWidget *vbox, int filter_type) {
    FILE *file = fopen("history.txt", "r");
    if (!file) {
        perror("fopen");
        return;
    }

    char line[512];
    char date[32] = "";
    gtk_container_foreach(GTK_CONTAINER(vbox), (GtkCallback)gtk_widget_destroy, NULL);

    while (fgets(line, sizeof(line), file)) {
        char col1[32], col2[32], col4[64], col5[32], date_temp[32];
        int col3;
        long long col5_data;

        // Đọc dòng chứa thông tin giao dịch
        if (sscanf(line, "%31s %31s %d %31s %lld", col1, col2, &col3, col4, &col5_data) != 5)
            continue;

        // Đọc dòng chứa chi tiết
        char detail[128];
        if (!fgets(detail, sizeof(detail), file))
            break;

        // Đọc dòng chứa ngày tháng
        if (!fgets(date_temp, sizeof(date_temp), file))
            break;

        // Loại bỏ ký tự xuống dòng từ chi tiết và ngày tháng
        detail[strcspn(detail, "\n")] = 0;
        date_temp[strcspn(date_temp, "\n")] = 0;

        // Kiểm tra xem ngày đã thay đổi chưa
        if (strcmp(date, date_temp) != 0) {
            strncpy(date, date_temp, sizeof(date) - 1);
            date[sizeof(date) - 1] = '\0';

            GtkWidget *date_label = gtk_label_new(date);
            PangoAttrList *attr_list_date = pango_attr_list_new();
            PangoAttribute *attr_size_date = pango_attr_size_new(14 * PANGO_SCALE);
            pango_attr_list_insert(attr_list_date, attr_size_date);
            gtk_label_set_attributes(GTK_LABEL(date_label), attr_list_date);
            gtk_box_pack_start(GTK_BOX(vbox), date_label, FALSE, FALSE, 5);
        }

        if (filter_type == 0 || filter_type == col3) {
            // Tạo chuỗi hiển thị từ các cột
            char display_line[256];
            if (col3 == 1) {
                if (atoi(col1) != 71) {
                    snprintf(display_line, sizeof(display_line),
                             "Người dùng %s máy số %s nạp %lld VND vào tài khoản lúc %s",
                             col2, col1, col5_data, col4);
                } else {
                    snprintf(display_line, sizeof(display_line),
                             "Người dùng %s nạp %lld VND vào tài khoản lúc %s",
                             col2, col5_data, col4);
                }
            } else if (col3 == 2) {
                snprintf(display_line, sizeof(display_line),
                         "Người dùng %s máy số %s đặt đồ ăn %s hết %lld VND vào lúc %s",
                         col2, col1, detail, col5_data, col4);
            } else {
                snprintf(display_line, sizeof(display_line),
                         "Người dùng %s máy số %s hành động không xác định vào lúc %s",
                         col2, col1, col4);
            }

            // Tạo một nhãn từ dòng phân tích được và thêm vào vbox
            GtkWidget *label = gtk_label_new(display_line);
            PangoAttrList *attr_list_label = pango_attr_list_new();
            PangoAttribute *attr_size_label = pango_attr_size_new(12 * PANGO_SCALE); // Cỡ chữ 12pt
            pango_attr_list_insert(attr_list_label, attr_size_label);
            gtk_label_set_attributes(GTK_LABEL(label), attr_list_label);
            gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        }
    }

    fclose(file);
    gtk_widget_show_all(vbox);
}

// Hàm cập nhật trang tổng doanh thu
void populate_revenue_page(GtkWidget *vbox) {
    FILE *file = fopen("history.txt", "r");
    if (!file) {
        perror("fopen");
        return;
    }

    char line[512];
    char previous_date[32] = "";
    long long total_deposit = 0;
    long long total_food = 0;

    gtk_container_foreach(GTK_CONTAINER(vbox), (GtkCallback)gtk_widget_destroy, NULL);

    while (fgets(line, sizeof(line), file)) {
        char col1[32], col2[32], col4[64], col5[32], date[32];
        int col3;
        long long col5_data;

        // Đọc dòng chứa thông tin giao dịch
        if (sscanf(line, "%31s %31s %d %31s %lld", col1, col2, &col3, col4, &col5_data) != 5)
            continue;

        // Bỏ qua chi tiết
        if (!fgets(line, sizeof(line), file))
            break;

        // Đọc dòng chứa ngày tháng
        if (!fgets(date, sizeof(date), file))
            break;

        // Loại bỏ ký tự xuống dòng từ ngày tháng
        date[strcspn(date, "\n")] = 0;

        if (strcmp(date, previous_date) != 0) {
            if (previous_date[0] != '\0') {
                char summary_line[512];
                snprintf(summary_line, sizeof(summary_line),
                         "Ngày %s:\nTổng nạp tiền: %lld VND\nTổng đặt đồ ăn: %lld VND",
                         previous_date, total_deposit, total_food);

                GtkWidget *summary_label = gtk_label_new(summary_line);
                PangoAttrList *attr_list = pango_attr_list_new();
                PangoAttribute *attr_size = pango_attr_size_new(12 * PANGO_SCALE);
                pango_attr_list_insert(attr_list, attr_size);
                gtk_label_set_attributes(GTK_LABEL(summary_label), attr_list);
                gtk_box_pack_start(GTK_BOX(vbox), summary_label, FALSE, FALSE, 5);

                total_deposit = 0;
                total_food = 0;
            }
            strncpy(previous_date, date, sizeof(previous_date) - 1);
            previous_date[sizeof(previous_date) - 1] = '\0';
        }

        if (col3 == 1) {
            total_deposit += col5_data;
        } else if (col3 == 2) {
            total_food += col5_data;
        }
    }

    if (previous_date[0] != '\0') {
        char summary_line[512];
        snprintf(summary_line, sizeof(summary_line),
                 "Ngày %s:\nTổng nạp tiền: %lld VND\nTổng đặt đồ ăn: %lld VND",
                 previous_date, total_deposit, total_food);

        GtkWidget *summary_label = gtk_label_new(summary_line);
        PangoAttrList *attr_list = pango_attr_list_new();
        PangoAttribute *attr_size = pango_attr_size_new(12 * PANGO_SCALE);
        pango_attr_list_insert(attr_list, attr_size);
        gtk_label_set_attributes(GTK_LABEL(summary_label), attr_list);
        gtk_box_pack_start(GTK_BOX(vbox), summary_label, FALSE, FALSE, 5);
    }

    fclose(file);
    gtk_widget_show_all(vbox);
}

static GtkWidget* create_transactions_page() {
    transactions_page_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    populate_transactions_page(transactions_page_vbox, 0);
    return transactions_page_vbox;
}

static GtkWidget* create_deposit_page() {
    deposit_page_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    populate_transactions_page(deposit_page_vbox, 1);
    return deposit_page_vbox;
}

static GtkWidget* create_food_page() {
    food_page_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    populate_transactions_page(food_page_vbox, 2);
    return food_page_vbox;
}

static GtkWidget* create_revenue_page() {
    revenue_page_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    populate_revenue_page(revenue_page_vbox);
    return revenue_page_vbox;
}


static GtkWidget* create_transactions_notebook() {
    GtkWidget *notebook = gtk_notebook_new();

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_transactions_page(), gtk_label_new("Tất cả"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_deposit_page(), gtk_label_new("Nạp tiền"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_food_page(), gtk_label_new("Đồ ăn"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_revenue_page(), gtk_label_new("Tổng doanh thu"));

    return notebook;
}

// Hàm kiểm tra và cập nhật tệp history.txt liên tục
gboolean check_file_update(gpointer data) {
    time_t mod_time = get_file_mod_time("history.txt");
    if (mod_time != last_mod_time) {
        last_mod_time = mod_time;
        // Cập nhật lại các trang
        populate_transactions_page(transactions_page_vbox, 0);
        populate_transactions_page(deposit_page_vbox, 1);
        populate_transactions_page(food_page_vbox, 2);
        populate_revenue_page(revenue_page_vbox); // Cập nhật trang tổng doanh thu
    }
    return TRUE; // Trả về TRUE để tiếp tục gọi lại hàm này
}


void updateFile() {
    FILE *f = fopen("products.txt", "w");
    
    for(int i = 0; i < number; i++) {
        fprintf(f, "%s\n%d %lld %s %s\n", 
                item[i].name, 
                item[i].quantity, 
                item[i].price, 
                item[i].dayIn, 
                item[i].Exp);
    }
    printf("Cap nhat du lieu thanh cong!\n");
    fclose(f);
}

gboolean update_order_page(gpointer){
    number = 0;
    free(item);
    item = NULL;
    getData();
    gtk_list_store_clear(store_address);
    for(int i = 0; i < number; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(store_address, &iter);   
        gtk_list_store_set(store_address, &iter,
                           0, i+1,
                           1, item[i].name,
                           2, item[i].quantity,
                           3, item[i].price,
                           4, item[i].dayIn,
                           5, item[i].Exp,
                           -1);
    }

    return TRUE;
}

void delete_item_on_page(GtkWidget *widget, gpointer data, const gchar *name){
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *id_entry, *quantity_entry;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;

    dialog = gtk_dialog_new_with_buttons("Xóa mặt hàng", GTK_WINDOW(data), flags, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();

    id_entry = gtk_entry_new();
    quantity_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ID mặt hàng: "), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), id_entry, 1, 0, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_OK) {
        const char *id = gtk_entry_get_text(GTK_ENTRY(id_entry));
        int id_number = atoi(id);
        if(id_number > number){
            GtkWidget *label = gtk_label_new("ID không hợp lệ!");
            gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 2, 1);       
            gtk_widget_show(label);
            gtk_widget_queue_draw(grid);

        }
        else{
            GtkTreeIter iter;
            gboolean valid;

            valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store_address), &iter);
            while (valid) {
                gint selected_id;
                gtk_tree_model_get(GTK_TREE_MODEL(store_address), &iter, 0, &selected_id, -1);

                if (selected_id == id_number) {
                    gtk_list_store_remove(store_address, &iter);
                    for(int j = id_number-1; j < number - 1; j++){
                        item[j] = item[j + 1];
                    }
                    number--;
                    item = (Item *)realloc(item, number * sizeof(Item));
                    updateFile();
                    break;
                }
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store_address), &iter);
            }
        }
    }
    gtk_widget_destroy(dialog);
    
}

void on_add_item(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *name_entry, *quantity_entry, *price_entry, *dayin_entry, *exp_entry;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;

    dialog = gtk_dialog_new_with_buttons("Thêm mặt hàng", GTK_WINDOW(data), flags, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();

    name_entry = gtk_entry_new();
    quantity_entry = gtk_entry_new();
    price_entry = gtk_entry_new();
    dayin_entry = gtk_entry_new();
    exp_entry = gtk_entry_new();

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tên mặt hàng:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Số lượng:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đơn giá:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Ngày nhập kho:"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dayin_entry, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Hạn sử dụng:"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), exp_entry, 1, 4, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_OK) {
        const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *quantity = gtk_entry_get_text(GTK_ENTRY(quantity_entry));
        const char *price = gtk_entry_get_text(GTK_ENTRY(price_entry));
        const char *dayin = gtk_entry_get_text(GTK_ENTRY(dayin_entry));
        const char *exp = gtk_entry_get_text(GTK_ENTRY(exp_entry));

        item = (Item *)realloc(item, (number + 1) * sizeof(Item));
        strcpy(item[number].name, name);
        item[number].quantity = atoi(quantity);
        item[number].price = atoll(price);
        strcpy(item[number].dayIn, dayin);
        strcpy(item[number].Exp, exp);
        number++;
        GtkTreeIter iter;
        gtk_list_store_append(store_address, &iter);
        gtk_list_store_set(store_address, &iter,
                           0, number, 
                           1, item[number - 1].name,
                           2, item[number - 1].quantity,
                           3, item[number - 1].price,
                           4, item[number - 1].dayIn,
                           5, item[number - 1].Exp,
                           -1);
        updateFile();
    }
    gtk_widget_destroy(dialog);
}

void on_add_old_item(GtkComboBox *widget, gpointer data) {
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *id_entry, *quantity_entry;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;

    dialog = gtk_dialog_new_with_buttons("Thêm mặt hàng", GTK_WINDOW(data), flags, "_OK", GTK_RESPONSE_OK, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();

    id_entry = gtk_entry_new();
    quantity_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ID mặt hàng: "), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), id_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Số lượng: "), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quantity_entry, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_OK) {
        const char *id = gtk_entry_get_text(GTK_ENTRY(id_entry));
        const char *quantity = gtk_entry_get_text(GTK_ENTRY(quantity_entry));
        int id_number = atoi(id);
        item[id_number-1].quantity += atoi(quantity);
        GtkTreeIter iter;
        gtk_list_store_append(store_address, &iter);
        gtk_list_store_set(store_address, &iter,
                        2, item[id_number-1].quantity,
                        -1);
        updateFile();
    }
    gtk_widget_show_all(dialog);
    gtk_widget_destroy(dialog);
}

void selection_box(GtkComboBox *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Loại sản phẩm", GTK_WINDOW(data), 
                                                    GTK_DIALOG_MODAL, "HỦY", GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkTreeIter iter;
    GtkListStore *store;
    GtkCellRenderer *renderer;

    GtkWidget *button_new = gtk_button_new_with_label("Sản phẩm mới");
    gtk_container_add(GTK_CONTAINER(content_area), button_new);
    g_signal_connect(button_new, "clicked", G_CALLBACK(on_add_item), NULL);

    GtkWidget *button_old = gtk_button_new_with_label("Sản phẩm cũ");
    gtk_container_add(GTK_CONTAINER(content_area), button_old);
    g_signal_connect(button_old, "clicked", G_CALLBACK(on_add_old_item), NULL);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static GtkWidget* create_orders_page() {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new("Thay đổi thông tin");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    GtkWidget *add_button = gtk_button_new_with_label("Thêm sản phẩm");
    gtk_box_pack_start(GTK_BOX(hbox), add_button, FALSE, FALSE, 0);
    g_signal_connect(add_button, "clicked", G_CALLBACK(selection_box), NULL);

    GtkWidget *del_product_button = gtk_button_new_with_label("Xóa sản phẩm");
    gtk_box_pack_start(GTK_BOX(hbox),del_product_button, FALSE, FALSE, 0);
    g_signal_connect(del_product_button, "clicked", G_CALLBACK(delete_item_on_page), NULL);

    GtkWidget *reload_button = gtk_button_new_with_label("Tải lại");
    gtk_box_pack_start(GTK_BOX(hbox),reload_button, FALSE, FALSE, 0);
    g_signal_connect(reload_button, "clicked", G_CALLBACK(update_order_page), NULL);

    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);

    GtkListStore *store = gtk_list_store_new(6, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_STRING);
    store_address = store;
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);

    for(int i = 0; i < number; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);   
        gtk_list_store_set(store, &iter,
                           0, i+1,
                           1, item[i].name,
                           2, item[i].quantity,
                           3, item[i].price,
                           4, item[i].dayIn,
                           5, item[i].Exp,
                           -1);
    }

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "ID", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Tên mặt hàng", gtk_cell_renderer_text_new(), "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Số lượng", gtk_cell_renderer_text_new(), "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Đơn giá", gtk_cell_renderer_text_new(), "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Ngày nhập kho", gtk_cell_renderer_text_new(), "text", 4, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Hạn sử dụng", gtk_cell_renderer_text_new(), "text", 5, NULL);

    g_timeout_add_seconds(5, update_order_page, NULL);

    return vbox;
}

gboolean read_ordered_file(){
    FILE *file = fopen("don_hang.txt", "r");
    if (file == NULL) {
        printf("Khong the mo tep.\n");
        return TRUE;
    }
    struct stat file_stat;

    if (stat("don_hang.txt", &file_stat) == 0) {
        if (file_stat.st_size > last_position) {
            int current_index = queue_index;

            fseek(file, last_position, SEEK_SET);
            char line[256];

            while (fgets(line, sizeof(line), file) != NULL) {
                queue_index++;
                queue = (QueueOrder *)realloc(queue, queue_index * sizeof(QueueOrder));
                sscanf(line, "%d", &id[queue_index - 1]);
                fgets(line, sizeof(line), file);
                line[strlen(line) - 1] = '\0';
                strcpy(queue[queue_index - 1].name, line);
                fgets(line, sizeof(line), file);
                sscanf(line, "%d %d", &queue[queue_index - 1].quantity, &queue[queue_index - 1].price);
                fgets(line, sizeof(line), file);
                line[strlen(line) - 1] = '\0';
                strcpy(queue[queue_index - 1].status, line);
                fgets(line, sizeof(line), file);
                line[strlen(line) - 1] = '\0';
                strcpy(queue[queue_index - 1].local_time, line);
            }
            last_position = ftell(file);

            fclose(file);        

            for(int i = 0; i < queue_index - current_index; i++) {
                GtkTreeIter iter;
                gtk_list_store_append(queue_address, &iter);   
                gtk_list_store_set(queue_address, &iter,
                                0, id[current_index + i],
                                1, queue[current_index + i].name,
                                2, queue[current_index + i].quantity,
                                3, queue[current_index + i].price * queue[current_index + i].quantity,
                                4, queue[current_index + i].status,
                                5, queue[current_index + i].local_time,
                                -1);
            }
        }
    } else {
        printf("None new\n");
    }
    fclose(file); 
    return TRUE;    
}

void confirm_clicked(GtkWidget *button, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkTreeModel *model;

    // Lấy model từ selection
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));

    // Kiểm tra xem có lựa chọn nào không
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int id;
        char *name;
        int quantity;
        int price;
        char *status;
        char *local_time;

        gtk_tree_model_get(model, &iter,
                           0, &id,
                           1, &name,
                           2, &quantity,
                           3, &price,
                           4, &status,
                           5, &local_time,
                           -1);
        
        update_history_file(so_may, current_username, 2, quantity * price, name);

        // Xóa hàng được chọn
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        
        g_free(name);
        g_free(status);
        g_free(local_time);
    }
}

static GtkWidget* create_order_queue_page(){
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);

    queue_address = gtk_list_store_new(6, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(queue_address));
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);

    GtkWidget *label = gtk_label_new("Xác thực trạng thái");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    GtkWidget *confirm_button = gtk_button_new_with_label("Xác nhận");
    gtk_box_pack_start(GTK_BOX(hbox), confirm_button, FALSE, FALSE, 0);
    g_signal_connect(confirm_button, "clicked", G_CALLBACK(confirm_clicked), tree_view);

    GtkWidget *reject_button = gtk_button_new_with_label("Từ chối đơn hàng");
    gtk_box_pack_start(GTK_BOX(hbox), reject_button, FALSE, FALSE, 0);
    g_signal_connect(reject_button, "clicked", G_CALLBACK(NULL), NULL);

    GtkWidget *reload_button = gtk_button_new_with_label("Tải lại");
    gtk_box_pack_start(GTK_BOX(hbox),reload_button, FALSE, FALSE, 0);
    g_signal_connect(reload_button, "clicked", G_CALLBACK(read_ordered_file), NULL);

    read_ordered_file();

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Số máy", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Tên mặt hàng", gtk_cell_renderer_text_new(), "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Số lượng", gtk_cell_renderer_text_new(), "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Tổng giá", gtk_cell_renderer_text_new(), "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Trạng thái", gtk_cell_renderer_text_new(), "text", 4, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Thời gian", gtk_cell_renderer_text_new(), "text", 5, NULL);
   
    g_timeout_add_seconds(2, read_ordered_file, NULL);

    return vbox;    
}

// Callback function for adding funds to an account
static void on_add_funds_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Nạp tiền vào tài khoản",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        ("Nạp tiền"), GTK_RESPONSE_ACCEPT,
        ("Hủy"), GTK_RESPONSE_REJECT,
        NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    GtkWidget *label_username = gtk_label_new("Tên tài khoản:");
    GtkWidget *entry_username = gtk_entry_new();
    GtkWidget *label_amount = gtk_label_new("Số tiền:");
    GtkWidget *entry_amount = gtk_entry_new();

    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_amount, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_amount, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username));
        const char *amount_str = gtk_entry_get_text(GTK_ENTRY(entry_amount));
        long long amount = atoll(amount_str);
        add_funds_to_account(username, amount);

        // Update interface
        GtkListStore *store = GTK_LIST_STORE(data);
        read_accounts(store);
    }

    gtk_widget_destroy(dialog);
}

// Hàm để cập nhật mật khẩu trong file "taikhoan.txt"
void update_password_in_file(const char *username, const char *new_password) {
    FILE *file = fopen("taikhoan.txt", "r+");
    if (file == NULL) {
        g_print("Không thể mở file taikhoan.txt để đọc và ghi.\n");
        return;
    }

    Account accounts[100]; // Giả định tối đa có 100 tài khoản
    int account_count = 0;
    int found = 0;

    // Đọc từng dòng trong file
    while (fscanf(file, "%s %s %s %lld\n", accounts[account_count].username, accounts[account_count].password,
                  accounts[account_count].id, &accounts[account_count].balance) != EOF) {
        if (strcmp(accounts[account_count].username, username) == 0) {
            strcpy(accounts[account_count].password, new_password); // Cập nhật mật khẩu mới
            found = 1;
        }
        account_count++;
    }

    if (found) {
        freopen("taikhoan.txt", "w", file); // Mở lại file để ghi
        for (int i = 0; i < account_count; i++) {
            fprintf(file, "%s %s %s %lld\n", accounts[i].username, accounts[i].password,
                    accounts[i].id, accounts[i].balance);
        }
    } else {
        g_print("Không tìm thấy tên tài khoản.\n");
    }

    fclose(file); // Đóng file
}

// Hàm để xử lý việc thay đổi mật khẩu
static void change_password(GtkWidget *dialog, GtkWidget *entry_username, GtkWidget *entry_new_password) {
    const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username)); // Lấy tên tài khoản
    const char *new_password = gtk_entry_get_text(GTK_ENTRY(entry_new_password)); // Lấy mật khẩu mới

    // Cập nhật mật khẩu trong file
    update_password_in_file(username, new_password);
    //Hiển thị khi đổi mật khẩu thành công
    GtkWidget *message_dialog = gtk_message_dialog_new(NULL,
                                                       GTK_DIALOG_MODAL,
                                                       GTK_MESSAGE_INFO,
                                                       GTK_BUTTONS_OK,
                                                       "Đổi mật khẩu thành công.");
    gtk_dialog_run(GTK_DIALOG(message_dialog));
    gtk_widget_destroy(message_dialog);

}

// Callback function for changing password
static void on_change_password_button_clicked(GtkWidget *widget, gpointer data) {
    // Tạo một hộp thoại mới với các nút "Đổi mật khẩu" và "Hủy"
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Đổi mật khẩu",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        ("Đổi mật khẩu"), GTK_RESPONSE_ACCEPT,
        ("Hủy"), GTK_RESPONSE_REJECT,
        NULL);

    // Lấy vùng nội dung của hộp thoại
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    // Tạo các nhãn và ô nhập liệu
    GtkWidget *label_username = gtk_label_new("Tên tài khoản:");
    GtkWidget *entry_username = gtk_entry_new();
    GtkWidget *label_new_password = gtk_label_new("Mật khẩu mới:");
    GtkWidget *entry_new_password = gtk_entry_new();

    gtk_entry_set_visibility(GTK_ENTRY(entry_new_password), FALSE); // Ẩn mật khẩu khi nhập

    // Sắp xếp các nhãn và ô nhập liệu vào lưới
    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_new_password, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_new_password, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid); // Thêm lưới vào vùng nội dung
    gtk_widget_show_all(dialog); // Hiển thị tất cả các widget trong hộp thoại

    // // Gửi con trỏ đến cả hai ô nhập liệu đến hàm xử lý
    // GtkWidget *entries[] = {entry_username, entry_new_password};
    // g_signal_connect(entry_username, "activate", G_CALLBACK(change_password), entries);
    // g_signal_connect(entry_new_password, "activate", G_CALLBACK(change_password), entries);

    // Nếu người dùng nhấn nút "Đổi mật khẩu"
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        change_password(dialog, entry_username, entry_new_password); //Cập nhật mật khẩu 
    }

    gtk_widget_destroy(dialog); // Hủy hộp thoại sau khi hoàn tất
}

static GtkWidget* create_accounts_page() {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *add_account_button = gtk_button_new_with_label("Thêm tài khoản");
    gtk_box_pack_start(GTK_BOX(hbox), add_account_button, FALSE, FALSE, 0);

    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);

    GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT64); // Thêm cột Căn cước công dân
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);

    GtkWidget *add_funds_button = gtk_button_new_with_label("Nạp tiền vào tài khoản");
    gtk_box_pack_start(GTK_BOX(hbox), add_funds_button, FALSE, FALSE, 0);
    g_signal_connect(add_funds_button, "clicked", G_CALLBACK(on_add_funds_button_clicked), store);

    GtkWidget *change_password_button = gtk_button_new_with_label("Đổi mật khẩu");
    gtk_box_pack_start(GTK_BOX(hbox), change_password_button, FALSE, FALSE, 0);
    g_signal_connect(change_password_button, "clicked", G_CALLBACK(on_change_password_button_clicked), store);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Tên", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Căn cước công dân", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Số tiền", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    read_accounts(store);
    g_signal_connect(add_account_button, "clicked", G_CALLBACK(on_add_account_button_clicked), store);

    return vbox;
}


// Hàm tạo cửa sổ admin
GtkWidget* create_admin_window() {
    admin_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(admin_window), "Màn hình ADMIN");
    gtk_window_set_default_size(GTK_WINDOW(admin_window), 1366, 768); // Kích thước cố định cho cửa sổ ADMIN
    gtk_window_set_resizable(GTK_WINDOW(admin_window), FALSE); // Không cho phép thay đổi kích thước
    g_signal_connect(admin_window, "delete-event", G_CALLBACK(on_window_delete_event), NULL);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(admin_window), notebook);

    

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_machines_page(), gtk_label_new("Máy"));
     // Tạo trang "Giao dịch" và thêm GtkNotebook con vào trang này
    GtkWidget *transactions_notebook = create_transactions_notebook();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), transactions_notebook, gtk_label_new("Giao dịch"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_orders_page(), gtk_label_new("Sản phẩm"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_accounts_page(), gtk_label_new("Tài khoản"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_order_queue_page(), gtk_label_new("Hàng chờ"));

    gtk_widget_show_all(admin_window);

    // Đặt hàm kiểm tra và cập nhật tệp history.txt sau mỗi 2 giây
    g_timeout_add(2000, check_file_update, NULL);
    return admin_window;
}

static gboolean on_user_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    // Trả về TRUE để ngăn cửa sổ đóng lại
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// USER

void update_ordered_file(){
    FILE *file = fopen("don_hang.txt", "a");
    for(int i = 0; i < cart_index; i++){
        fprintf(file, "%d\n%s\n%d %d\n%s\n%s\n", 
                        so_may, 
                        ordered[ordered_index - cart_index + i].name,
                        ordered[ordered_index - cart_index + i].quantity,
                        ordered[ordered_index - cart_index + i].price,
                        ordered[ordered_index - cart_index + i].status,
                        ordered[ordered_index - cart_index + i].local_time);
    }
    fclose(file);
}

void timeUpdate(OrderBill *ordered){
    GDateTime *now = g_date_time_new_now_local();

    int year = g_date_time_get_year(now);
    int month = g_date_time_get_month(now);
    int day = g_date_time_get_day_of_month(now);
    int hour = g_date_time_get_hour(now);
    int minute = g_date_time_get_minute(now);
    int second = g_date_time_get_second(now);

    sprintf(ordered->local_time, "%02d-%02d-%04d %02d:%02d:%02d",
            day, month, year, hour, minute, second);

    g_date_time_unref(now);

    return;
}

// Hàm cập nhật box hiển thị những mặt hàng được order
 // Khai báo overlay toàn cục để dễ truy cập

void show_notification(GtkWidget *overlay, Order *cart) {
    static GtkWidget *notification_box = NULL;

    if (!notification_box) {
        notification_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_set_valign(notification_box, GTK_ALIGN_END);
        gtk_widget_set_halign(notification_box, GTK_ALIGN_END);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), notification_box);
        gtk_widget_show(notification_box);
    }
    char message[100];
    sprintf(message, "%s đã đạt giới hạn hàng tồn kho", cart->name);
    GtkWidget *label = gtk_label_new(message);
    gtk_widget_set_name(label, "notification-label");

    const gchar *css =
        "#notification-label {"
        "  background-color: rgba(0, 0, 0, 0.8);"
        "  color: white;"
        "  padding: 10px;"
        "  border-radius: 10px;"
        "  font-size: 20px;"
        "  transition: opacity 0.5s ease-in-out;"
        "  opacity: 1.0;"
        "}";

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(label);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);

    gtk_box_pack_start(GTK_BOX(notification_box), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    g_timeout_add(1500, (GSourceFunc) gtk_widget_destroy, label);
    return;
}
void update_ordered_box(GtkWidget *widget, gpointer data){
    ordered_index += cart_index;
    ordered = (OrderBill *)realloc(ordered, ordered_index * sizeof(OrderBill));
    for(int i = 0; i < cart_index; i++){
        strcpy(ordered[ordered_index - cart_index + i].name, cart[i].name);
        ordered[ordered_index - cart_index + i].quantity = cart[i].quantity;
        ordered[ordered_index - cart_index + i].price = cart[i].price;
        bill_total += cart[i].quantity * cart[i].price;

        strcpy(ordered[ordered_index - cart_index + i].status, "Chưa hoàn thành");
        timeUpdate(&ordered[ordered_index - cart_index + i]);
        gtk_widget_destroy(cart[i].all);
    }
    update_ordered_file();
    for(int i = 0; i < cart_index; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(ordered_address, &iter);   
        gtk_list_store_set(ordered_address, &iter,
                           0, ordered_index - cart_index + i + 1,
                           1, ordered[ordered_index - cart_index + i].name,
                           2, ordered[ordered_index - cart_index + i].quantity,
                           3, ordered[ordered_index - cart_index + i].price,
                           4, ordered[ordered_index - cart_index + i].status,
                           5, ordered[ordered_index - cart_index + i].local_time,
                           -1);
    }
    printf("%d\n", bill_total);
    free(cart);
    cart = NULL;
    cart_index = 0;
    bill_cost = 0;
    gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
    gtk_label_set_text(GTK_LABEL(bill_total_address), g_strdup_printf("%lld VND", bill_total));

    gtk_widget_show_all(user_window);
    gtk_widget_show_all(summary_box);
}

// Hàm cập nhật khi bấm nút " X "
void delete(GtkWidget *widget, gpointer *data){
    Order *cartx = (Order *)data;
    
    if (GTK_IS_WIDGET(cartx->all)) {
        gtk_widget_destroy(cartx->all);
    }
    gtk_widget_show_all(summary_box);

    bill_cost -= cartx->quantity * cartx->price;
    gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
    printf("Da bot %s khoi gio hang.\n", cartx->name);
    for(int i = 0; i < cart_index; i++){
        if(&cart[i] == cartx){
            printf("%d\n", i);
            for(int j = i; j < cart_index - 1; j++){
                cart[j] = cart[j + 1];
            }
            break;
        }
    }
    cart_index--;
    for(int i = 0; i < cart_index; i++){
        printf("%d\t%s\t%d\n", i+1, cart[i].name, cart[i].quantity);
    }
    gtk_widget_show_all(summary_box);
    return;
}

// Hàm tạo giao diện từng mặt hàng được order
GtkWidget *box_cart(Order *cart){
    GtkWidget *cart_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *cart_name = gtk_label_new(cart->name);
    GtkWidget *cart_quantity = gtk_label_new(g_strdup_printf("x%d", cart->quantity));
    GtkWidget *button_x = gtk_button_new_with_label("X");

    cart->quantity_widget = cart_quantity;
    gtk_box_pack_start(GTK_BOX(cart_box), cart_name, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(cart_box), cart_quantity, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(cart_box), button_x, TRUE, TRUE, 0);

    gtk_widget_set_halign(cart_name, GTK_ALIGN_START);
    gtk_widget_set_halign(cart_quantity, GTK_ALIGN_START);
    gtk_widget_set_halign(button_x, GTK_ALIGN_END);
    gtk_widget_set_size_request(button_x, 100, -1);

    g_signal_connect(button_x, "clicked", G_CALLBACK(delete), cart);
    gtk_widget_show_all(summary_box);
    return cart_box;
}

// Hàm cập nhật danh sách đơn hàng và tổng tiền thanh toán khi bấm nút " - " 
void update_minus(GtkWidget *widget, gpointer *data){
    Item *item = (Item *)data; 
    for(int i = 0; i < cart_index; i++){
        if (strcmp(item->name, cart[i].name) == 0) {
            if(cart[i].quantity > 1){
                cart[i].quantity--;
                bill_cost -= item->price;
                
                gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
                gtk_label_set_text(GTK_LABEL(cart[i].quantity_widget), g_strdup_printf("x%d", cart[i].quantity));

                printf("Da bot %s khoi gio hang.\n", cart[i].name);
                for(int i = 0; i < cart_index; i++){
                    printf("%d\t%s\t%d\n", i+1, cart[i].name, cart[i].quantity);
                }
                gtk_widget_show_all(summary_box);
                return;
            }
            else{
                bill_cost -= item->price;
                gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
                
                if (GTK_IS_WIDGET(cart[i].all)) {
                    gtk_widget_destroy(cart[i].all);
                }
                gtk_widget_show_all(summary_box);

                for(int j = i; j < cart_index; j++){
                    cart[j] = cart[j + 1];
                }
                cart_index--;
                cart = (Order *)realloc(cart, cart_index * sizeof(Order));
                
                gtk_widget_show_all(summary_box);
                return;
            }   
        }
    }
    gtk_label_set_text(GTK_LABEL(cart->quantity_widget), g_strdup_printf("x%d", cart->quantity));
    gtk_widget_show_all(summary_box);
}

// Hàm thêm đồ vào giỏ hàng
void add_to_cart(GtkWidget *widget, gpointer data) {
    Item *item = (Item *)data;  
    printf("Item name: %s, Address: %p\n", item->name, (void *)item); 
    for (int i = 0; i < cart_index; i++) {
        if (strcmp(item->name, cart[i].name) == 0) {
            if(cart[i].quantity < item->quantity){
                cart[i].quantity++;
                bill_cost += item->price;

                gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
                gtk_label_set_text(GTK_LABEL(cart[i].quantity_widget), g_strdup_printf("x%d", cart[i].quantity));
                gtk_widget_show_all(summary_box);

                printf("Da them %s vo gio hang.\n", cart[i].name);
                for(int i = 0; i < cart_index; i++){
                    printf("%d\t%s\t%d\n", i+1, cart[i].name, cart[i].quantity);
                }
                return;
            }
            else {
                show_notification(overlay, &cart[i]);
                return;
            }
        }
    }

    if (cart_index < 50) {
        cart = (Order *)realloc(cart, (cart_index+1) * sizeof(Order));
        strcpy(cart[cart_index].name, item->name);
        cart[cart_index].quantity = 1;
        cart[cart_index].price = item->price;
        cart_index++;
        printf("Da them %s vo gio hang.\n", item->name);

        GtkWidget *create_box = box_cart(&cart[cart_index-1]);
        cart[cart_index-1].all = create_box;
        gtk_box_pack_start(GTK_BOX(summary_box), create_box, TRUE, TRUE, 0);
        gtk_widget_show_all(summary_box);

        bill_cost += item->price;
        gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));

        } else {
        printf("Gio hang da day!\n");
    }
    for(int i = 0; i < cart_index; i++){
        printf("%d\t%s\t%d\n", i+1, cart[i].name, cart[i].quantity);
    }   
    
}

//Hàm tạo giao diện đặt đồ ăn
GtkWidget *create_food_item(Item *item) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget *frame = gtk_frame_new(NULL);
    GtkWidget *image = gtk_image_new_from_file(item->imagePath);
    GtkWidget *label_name = gtk_label_new(item->name);
    GtkWidget *label_price = gtk_label_new(g_strdup_printf("%lld VND", item->price));
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *button_minus = gtk_button_new_with_label("-");
    GtkWidget *button_plus = gtk_button_new_with_label("+");

    gtk_widget_set_size_request(button_minus, 100, -1);
    gtk_widget_set_size_request(button_plus, 100, -1);
    gtk_widget_set_hexpand(button_minus, TRUE);
    gtk_widget_set_hexpand(button_plus, TRUE);
    gtk_box_pack_start(GTK_BOX(button_box), button_plus, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), button_minus, FALSE, FALSE, 0);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, "button { font-size: 20px; }", -1, NULL);
    gtk_widget_set_size_request(button_box, -1, gtk_widget_get_allocated_height(frame));
    
    GtkStyleContext *context_minus = gtk_widget_get_style_context(button_minus);
    GtkStyleContext *context_plus = gtk_widget_get_style_context(button_plus);
    gtk_style_context_add_provider(context_minus, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_style_context_add_provider(context_plus, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_widget_set_size_request(frame, 225, 225);
    gtk_container_add(GTK_CONTAINER(frame), image);
    g_signal_connect(button_plus, "clicked", G_CALLBACK(add_to_cart), item);
    g_signal_connect(button_minus, "clicked", G_CALLBACK(update_minus), item);

    gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label_price, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), button_box, FALSE, FALSE, 10);

    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);

    return box;
}

gboolean on_display_close(GtkWidget *widget, GdkEvent *event, gpointer data) {
    free(cart);
    cart = NULL;
    cart_index = 0;
    bill_cost = 0;
    gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
    return FALSE;
}

void create_food_display(GtkWidget *window, gpointer data) {
    GtkWidget *dialog, *content_area;
    GtkWidget *scrolled_window;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;
    GtkWidget *grid;
    GtkWidget *frame;

    dialog = gtk_dialog_new_with_buttons("GIAO DIỆN NGƯỜI DÙNG", GTK_WINDOW(data), flags, "_Close", GTK_RESPONSE_CLOSE, NULL);
    g_signal_connect(dialog, "delete_event", G_CALLBACK(on_display_close), dialog);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1500, 500);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    overlay = gtk_overlay_new(); // Khởi tạo overlay
    gtk_container_add(GTK_CONTAINER(content_area), overlay);
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled_window, 1000, 500);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(scrolled_window), grid);
    gtk_container_add(GTK_CONTAINER(overlay), scrolled_window);
    

    for (int i = 0; i < number; i++) {
        GtkWidget *food_item = create_food_item(&item[i]);
        gtk_grid_attach(GTK_GRID(grid), food_item, i % 5, i / 5, 1, 1);
    }

    summary_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    frame = gtk_frame_new("THÔNG TIN HÓA ĐƠN");
    gtk_frame_set_label_align(GTK_FRAME(frame), 0.5, 0.5); // Align label center
    gtk_container_add(GTK_CONTAINER(content_area), frame);

    GtkWidget *bill_button = gtk_button_new_with_label("THANH TOÁN");
    GtkWidget *bill_pay = gtk_label_new(g_strdup_printf("%lld VND", bill_cost));
    GtkWidget *bill_frame = gtk_frame_new(NULL);

    gtk_container_add(GTK_CONTAINER(bill_frame), bill_pay);
    bill_address = bill_pay;
    PangoFontDescription *font_desc;
    font_desc = pango_font_description_from_string("Sans 30");
    gtk_widget_override_font(bill_pay, font_desc);

    gtk_box_pack_end(GTK_BOX(summary_box), bill_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(summary_box), bill_frame, FALSE, FALSE, 0);
    g_signal_connect(bill_button, "clicked", G_CALLBACK(update_ordered_box), NULL);

    gtk_widget_set_halign(bill_button, GTK_ALIGN_END);
    gtk_widget_set_halign(bill_pay, GTK_ALIGN_END);

    gtk_container_add(GTK_CONTAINER(content_area), summary_box);
    

    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_CLOSE){
        free(cart);
        cart = NULL;
        cart_index = 0;
        bill_cost = 0;
        gtk_label_set_text(GTK_LABEL(bill_address), g_strdup_printf("%lld VND", bill_cost));
    }

    gtk_widget_destroy(dialog);
}

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer data) {
    UserData *user_data = (UserData *)data;

    if (response_id == GTK_RESPONSE_OK) {
        // Mở khóa máy khi nhấn OK
        user_data->locked = FALSE;

        // Khởi động lại timeout cập nhật giao diện
        if (user_data->timeout_id == 0) {
            user_data->timeout_id = g_timeout_add_seconds(1, update_user_window, user_data);
        }
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_lock_button_clicked(GtkWidget *widget, gpointer data) {
    UserData *user_data = (UserData *)data;

    if (!user_data->locked) {
        // Đang không khóa, bây giờ khóa máy
        user_data->locked = TRUE;

        // Ngắt timeout nếu có
        if (user_data->timeout_id > 0) {
            g_source_remove(user_data->timeout_id);
            user_data->timeout_id = 0;
        }

        // Cập nhật trạng thái "pause" trong file
        update_monitor_file(so_may, user_data->user_account->username, user_data->user_account->id, "pause", user_data->balance, user_data->rate, user_data->date_str, user_data->time_str);

        // Hiển thị hộp thoại thông báo khóa máy
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(user_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Máy đã bị khóa. Nhấn OK để tiếp tục.");
        gtk_window_set_title(GTK_WINDOW(dialog), "Máy đã bị khóa");
        g_signal_connect(GTK_DIALOG(dialog), "response", G_CALLBACK(on_dialog_response), user_data);
        gtk_widget_show(dialog);
    } else {
        // Đang khóa, bây giờ mở khóa máy
        user_data->locked = FALSE;

        // Khởi động lại timeout cập nhật giao diện
        if (user_data->timeout_id == 0) {
            user_data->timeout_id = g_timeout_add_seconds(1, update_user_window, user_data);
        }

        // Cập nhật trạng thái "on" trong file
        update_monitor_file(so_may, user_data->user_account->username, user_data->user_account->id, "on", user_data->balance, user_data->rate, user_data->date_str, user_data->time_str);

        // Hiển thị hộp thoại thông báo mở khóa máy
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(user_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Máy đã được mở khóa. Nhấn OK để tiếp tục.");
        gtk_window_set_title(GTK_WINDOW(dialog), "Máy đã được mở khóa");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}



// Hàm xử lý sự kiện nhấn nút "Chuyển tiền"
void on_transfer_confirm_button_clicked(GtkWidget *widget, gpointer data) {
    TransferData *transfer_data = (TransferData *)data;
    UserData *user_data = transfer_data->user_data;

    const gchar *account_to_transfer = gtk_entry_get_text(GTK_ENTRY(transfer_data->transfer_account_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(transfer_data->password_entry));
    const gchar *amount_str = gtk_entry_get_text(GTK_ENTRY(transfer_data->amount_entry));
    long long amount = atoll(amount_str);

    gchar *message;
    GList *accounts_list = read_accounts_from_file("taikhoan.txt");
    gboolean transfer_success = FALSE;

    for (GList *l = accounts_list; l != NULL; l = l->next) {
        Account *account = (Account *)l->data;

        if (g_strcmp0(user_data->user_account->username, account->username) == 0 && g_strcmp0(user_data->user_account->password, password) == 0) {
            if (amount > 0 && account->balance >= amount) {
                account->balance -= amount;
                user_data->balance -= amount;
                for (GList *l_dest = accounts_list; l_dest != NULL; l_dest = l_dest->next) {
                    Account *dest_account = (Account *)l_dest->data;
                    if (g_strcmp0(account_to_transfer, dest_account->username) == 0) {
                        dest_account->balance += amount;
                        transfer_success = TRUE;
                        break;
                    }
                }
                if (transfer_success) {
                    write_accounts_to_file("taikhoan.txt", accounts_list);
                    message = g_strdup_printf("Chuyển tiền thành công %lld đến tài khoản %s", amount, account_to_transfer);
                } else {
                    message = g_strdup("Tài khoản nhận không tồn tại.");
                }
            } else {
                message = g_strdup("Số dư không đủ hoặc số tiền không hợp lệ.");
            }
            break;
        }
    }

    if (!transfer_success) {
        message = g_strdup("Sai tên tài khoản hoặc mật khẩu.");
    }

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_free(message);
    g_list_free_full(accounts_list, (GDestroyNotify)g_slice_free1); // Giải phóng bộ nhớ của danh sách tài khoản
}

// Hàm tạo cửa sổ chuyển tiền
static void on_transfer_button_clicked(GtkWidget *widget, gpointer data) {
    UserData *user_data = (UserData *)data;

    // Tạo cửa sổ mới cho form chuyển tiền
    GtkWidget *transfer_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(transfer_window), "Chuyển tiền");
    gtk_window_set_default_size(GTK_WINDOW(transfer_window), 400, 300);
    gtk_window_set_position(GTK_WINDOW(transfer_window), GTK_WIN_POS_CENTER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(transfer_window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // Tạo nhãn và ô nhập cho tài khoản muốn chuyển
    GtkWidget *transfer_account_label = gtk_label_new("Tài khoản muốn chuyển:");
    gtk_box_pack_start(GTK_BOX(vbox), transfer_account_label, FALSE, FALSE, 0);

    GtkWidget *transfer_account_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), transfer_account_entry, FALSE, FALSE, 0);

    // Tạo nhãn và ô nhập cho mật khẩu
    GtkWidget *password_label = gtk_label_new("Mật khẩu:");
    gtk_box_pack_start(GTK_BOX(vbox), password_label, FALSE, FALSE, 0);

    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE); // Ẩn mật khẩu
    gtk_box_pack_start(GTK_BOX(vbox), password_entry, FALSE, FALSE, 0);

    // Tạo nhãn và ô nhập cho số tiền
    GtkWidget *amount_label = gtk_label_new("Số tiền:");
    gtk_box_pack_start(GTK_BOX(vbox), amount_label, FALSE, FALSE, 0);

    GtkWidget *amount_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), amount_entry, FALSE, FALSE, 0);

    // Tạo nút chuyển tiền
    GtkWidget *transfer_button = gtk_button_new_with_label("Chuyển tiền");
    gtk_box_pack_start(GTK_BOX(vbox), transfer_button, FALSE, FALSE, 0);

    // Tạo cấu trúc dữ liệu để truyền cho hàm xử lý nút chuyển tiền
    TransferData *transfer_data = g_new(TransferData, 1);
    transfer_data->transfer_account_entry = transfer_account_entry;
    transfer_data->password_entry = password_entry;
    transfer_data->amount_entry = amount_entry;
    transfer_data->user_data = user_data; // Truyền thêm UserData

    // Kết nối sự kiện cho nút chuyển tiền
    g_signal_connect(transfer_button, "clicked", G_CALLBACK(on_transfer_confirm_button_clicked), transfer_data);

    gtk_widget_show_all(transfer_window);
}

GtkWidget* create_user_window(gdouble initial_balance, gint machines, Account *user_account, GList *accounts_list) {
    user_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(user_window), "Màn hình User");
    gtk_window_set_default_size(GTK_WINDOW(user_window), 450, 1000); // Kích thước cố định cho cửa sổ USER
    gtk_window_set_resizable(GTK_WINDOW(user_window), TRUE); // Không cho phép thay đổi kích thước
     // Cài đặt sự kiện delete-event để ngăn không cho cửa sổ đóng
    g_signal_connect(user_window, "delete-event", G_CALLBACK(on_user_window_delete_event), NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(user_window), grid);

    GtkWidget *label_language = gtk_label_new("Ngôn ngữ:");
    GtkWidget *combo_language = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_language), "Tiếng Việt");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_language), "English");

    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_language), 0);

    GtkWidget *label_total = gtk_label_new("Tổng thanh toán:");
    GtkWidget *entry_total = gtk_label_new(" 0 VND");

    GtkWidget *label_usage = gtk_label_new("Sử dụng:");
    GtkWidget *entry_usage_time = gtk_label_new("00:00:00");
    GtkWidget *entry_usage_amount = gtk_label_new(" 0 VND");


    GtkWidget *label_balance = gtk_label_new("Còn lại:");
    GtkWidget *time_label = gtk_label_new("00:00:00");
    GtkWidget *balance_label = gtk_label_new(" 00000 VND");

    GtkWidget *label_service_fee = gtk_label_new("Phí dịch vụ (VND):");
    GtkWidget *entry_service_fee = gtk_label_new(" 0 VND");

    GtkWidget *progress_bar = gtk_progress_bar_new();
    GtkWidget *progress_label = gtk_label_new("100 %");

    GtkWidget *button_service = gtk_button_new_with_label("Dịch vụ");
    GtkWidget *button_utilities = gtk_button_new_with_label("Tiện ích");
    GtkWidget *button_lock = gtk_button_new_with_label("Khóa máy");
    GtkWidget *button_customer = gtk_button_new_with_label("Khách hàng");
    GtkWidget *button_communication = gtk_button_new_with_label("Giao tiếp");

    ordered_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *ordered_label = gtk_label_new("Tổng đơn hàng");
    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkListStore *store = gtk_list_store_new(6, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    ordered_address = store;
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "ID", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Tên mặt hàng", gtk_cell_renderer_text_new(), "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Số lượng", gtk_cell_renderer_text_new(), "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Đơn giá", gtk_cell_renderer_text_new(), "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Trạng thái", gtk_cell_renderer_text_new(), "text", 4, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Thời gian", gtk_cell_renderer_text_new(), "text", 5, NULL);
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);

    GtkWidget *bill_label = gtk_label_new("Tổng thanh toán");
    GtkWidget *bill_total_label =  gtk_label_new(g_strdup_printf("%lld VND", bill_total));
    bill_total_address = bill_total_label;

    gtk_grid_attach(GTK_GRID(grid), label_language, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), combo_language, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_total, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_total, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_usage, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_usage_time, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_usage_amount, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_balance, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), time_label, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), balance_label, 2, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), label_service_fee, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_service_fee, 1, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), progress_bar, 0, 7, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), progress_label, 0, 8, 3, 1);

    GtkWidget *logout_button = gtk_button_new_with_label("Đăng xuất");
    gtk_grid_attach(GTK_GRID(grid), logout_button, 1, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button_service, 0, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button_utilities, 2, 9, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), button_lock, 0, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button_customer, 1, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button_communication, 2, 10, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ordered_label,0, 11, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), ordered_box, 0, 12, 3, 4);
    gtk_grid_attach(GTK_GRID(grid), ordered_box, 0, 12, 3, 4);
    gtk_grid_attach(GTK_GRID(grid), bill_label, 0, 16, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), bill_total_label, 0, 17, 3, 1);
    gtk_widget_set_vexpand(ordered_box, TRUE);
    gtk_widget_set_hexpand(ordered_box, TRUE);
    gtk_box_pack_start(GTK_BOX(ordered_box), scroll_window, TRUE, TRUE, 0);
    gtk_widget_set_vexpand(bill_total_label, TRUE);

    UserData *user_data = g_slice_new(UserData);
    user_data->time_label = time_label;
    user_data->balance_label = balance_label;
    user_data->progress_bar = progress_bar;
    user_data->progress_label = progress_label;
    user_data->entry_usage_time = entry_usage_time;
    user_data->usage_amount = entry_usage_amount;
    user_data->entry_total = entry_total;
    user_data->balance = initial_balance;
    user_data->initial_balance = initial_balance;
    user_data->locked = FALSE;
    if (machines > 0 && machines < 21) {
        user_data->rate = 8000;
    } else if (machines > 20 && machines < 41) {
        user_data->rate = 10000;
    } else if (machines > 40 && machines < 61) {
        user_data->rate = 15000;
    } else if (machines > 61 && machines < 71) {
        user_data->rate = 20000;
    } 
    GDateTime *now = g_date_time_new_now_local();

    // Lấy thời gian hiện tại
    int year = g_date_time_get_year(now);
    int month = g_date_time_get_month(now);
    int day = g_date_time_get_day_of_month(now);
    int hour = g_date_time_get_hour(now);
    int minute = g_date_time_get_minute(now);
    int second = g_date_time_get_second(now);

    // Định dạng ngày tháng vào chuỗi
    g_snprintf(user_data->date_str, sizeof(user_data->date_str), "%02d-%02d-%04d", day, month, year);

    // Định dạng giờ phút giây vào chuỗi
    g_snprintf(user_data->time_str, sizeof(user_data->time_str), "%02d:%02d:%02d", hour, minute, second);

    g_date_time_unref(now);
    
    g_signal_connect(logout_button, "clicked", G_CALLBACK(on_logout_button_clicked), user_data);

    g_signal_connect(button_lock, "clicked", G_CALLBACK(on_lock_button_clicked), user_data);

    g_signal_connect(button_service, "clicked", G_CALLBACK(create_food_display),user_window);
    user_data->user_account = user_account;
    user_data->accounts_list = accounts_list;
    user_data->timeout_id = g_timeout_add_seconds(1, update_user_window, user_data);

    gtk_widget_set_hexpand(grid, TRUE); // Mở rộng ngang
    gtk_widget_set_vexpand(grid, TRUE); // Mở rộng dọc

    gtk_widget_show_all(user_window);
    gtk_widget_hide(ordered_box);

    return user_window;
}

void calculate_time_use(gint *h, gint *m, gint *s, gint hours, gint min, gint sec){
    *s+= +1;
    if(*s == 60){
        *m+=1  ;
        *s =0;
        if(*m == 60){
            *h+= 1;
            *m = 0;
        }
    }
}

// Hàm cập nhật tệp "taikhoan.txt"
void update_account_file(const char *username, const char *password, const char *id, long long balance) {
    FILE *file = fopen("taikhoan.txt", "r+");
    if (file) {
        char lines[256][256]; // assuming a maximum of 256 accounts with each line up to 256 characters
        int line_count = 0;

        // Đọc toàn bộ dòng trong tệp vào mảng
        while (fgets(lines[line_count], sizeof(lines[line_count]), file)) {
            line_count++;
        }

        rewind(file);

        // Tìm và cập nhật thông tin tài khoản
        for (int i = 0; i < line_count; i++) {
            if (g_str_has_prefix(lines[i], username)) {
                snprintf(lines[i], sizeof(lines[i]), "%s %s %s %lld\n", username, password, id, balance);
            }
        }

        // Ghi lại toàn bộ nội dung đã cập nhật vào tệp
        for (int i = 0; i < line_count; i++) {
            fprintf(file, "%s", lines[i]);
        }

        fclose(file);
    } else {
        g_warning("Không thể mở tệp taikhoan.txt để đọc và cập nhật.");
    }
}


gint hours_use =  0, minutes_use = 0, seconds_use = 0;
// Hàm cập nhật giao diện người dùng
static gboolean update_user_window(gpointer data) {
    UserData *user_data = (UserData *)data;
    
    // Cập nhật số dư tài khoản theo thời gian
    user_data->balance -= user_data->rate * (1.0 / 3600.0);
    
    // Cập nhật tệp taikhoan.txt
    update_account_file(user_data->user_account->username,
                        user_data->user_account->password,
                        user_data->user_account->id,
                        user_data->balance);


    if (user_data->locked) {
        // Nếu khóa, không cập nhật giao diện
        return TRUE;
    }
    
    // Tính toán thời gian còn lại
    gint hours, minutes, seconds;
    calculate_time_remaining(&hours, &minutes, &seconds, user_data->balance, user_data->rate);

    //cập nhật thời gian sử dụng

    calculate_time_use(&hours_use,&minutes_use,&seconds_use , hours , minutes , seconds);
    
    gchar *timeuse_str = g_strdup_printf("%02d:%02d:%02d", hours_use,  minutes_use, seconds_use);
    gtk_label_set_text(GTK_LABEL(user_data->entry_usage_time), timeuse_str);
    g_free(timeuse_str);

    gchar *balanceuse_str = g_strdup_printf(" %.0f VND", user_data->initial_balance - user_data->balance);
    gtk_label_set_text(GTK_LABEL(user_data->usage_amount), balanceuse_str);
    g_free(balanceuse_str);


    // Cập nhật nhãn thời gian và số dư
    gchar *time_str = g_strdup_printf("%02d:%02d:%02d", hours, minutes, seconds);
    gtk_label_set_text(GTK_LABEL(user_data->time_label), time_str);
    g_free(time_str);

    gchar *balance_str = g_strdup_printf(" %.0f VND", user_data->balance);
    gtk_label_set_text(GTK_LABEL(user_data->balance_label), balance_str);
    g_free(balance_str);

    // Cập nhật tiến trình
    gdouble progress_fraction = user_data->balance / user_data->initial_balance;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(user_data->progress_bar), progress_fraction);
    gchar *progress_str = g_strdup_printf("%.2f%%", progress_fraction * 100);
    gtk_label_set_text(GTK_LABEL(user_data->progress_label), progress_str);
    g_free(progress_str);

    // Cập nhật file theodoi.txt
    update_monitor_file(so_may, user_data->user_account->username, user_data->user_account->id, "on", user_data->balance, user_data->rate,  user_data->date_str, user_data->time_str);


    // Kiểm tra nếu hết số dư
    if (user_data->balance <= 0.0) {
        // Ngừng cập nhật giao diện
        if (user_data->timeout_id > 0) {
            g_source_remove(user_data->timeout_id);
            user_data->timeout_id = 0;
        }
        // Cập nhật nhãn thời gian và số dư
        gchar *time_str = g_strdup_printf("00:00:00");
        gtk_label_set_text(GTK_LABEL(user_data->time_label), time_str);
        g_free(time_str);
        gchar *balance_str = g_strdup_printf(" 0 VND");
        gtk_label_set_text(GTK_LABEL(user_data->balance_label), balance_str);
        g_free(balance_str);
        gchar *progress_str = g_strdup_printf("0 %");
        gtk_label_set_text(GTK_LABEL(user_data->progress_label), progress_str);
        g_free(progress_str);
        user_data->balance = 0;
        // Hiển thị thông báo hết thời gian sử dụng
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(user_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Số dư đã hết. Vui lòng nạp thêm tiền để tiếp tục sử dụng.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        // Cập nhật trạng thái "off" cho máy
        update_monitor_file(so_may, "x", "x", "off", 0, user_data->rate,"x","x");

        // Đăng xuất người dùng
        on_logout_button_clicked(NULL, user_data);

        return FALSE; // Dừng hàm gọi lại
    }

    return TRUE; // Tiếp tục gọi lại hàm này
}

// Hàm xử lý sự kiện khi nhấn nút đăng nhập
void on_login_button_clicked(GtkWidget *widget, gpointer data) {
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(((GtkWidget **)data)[0]));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(((GtkWidget **)data)[1]));

    GList *accounts_list = read_accounts_from_file("taikhoan.txt");
    gboolean login_success = FALSE;

    for (GList *l = accounts_list; l != NULL; l = l->next) {
        Account *account = (Account *)l->data;
        if (g_strcmp0(username, account->username) == 0 && g_strcmp0(password, account->password) == 0) {
            strcpy(current_username, username);
            login_success = TRUE;
            // Sao chép mật khẩu vào mảng password
            strncpy(password_user, password, sizeof(password_user));
             if (so_may >= 1 && so_may <= 70) {
                if (g_strcmp0(username, "admin") == 0) {
                    // Đăng nhập admin trên máy không phải 71
                    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_ERROR,
                                                               GTK_BUTTONS_CLOSE,
                                                               "Máy không có quyền truy cập admin.");
                    gtk_window_set_title(GTK_WINDOW(dialog), "Lỗi đăng nhập");
                    gtk_dialog_run(GTK_DIALOG(dialog));
                    gtk_widget_destroy(dialog);
                    login_success = FALSE;
                } else {
                    // Đăng nhập user trên máy không phải 71
                    user_window = create_user_window(account->balance, so_may, account, accounts_list);
                    gtk_widget_show_all(user_window);
                }
            } else if (so_may == 71) {
                if (g_strcmp0(username, "admin") != 0) {
                    // Đăng nhập user trên máy 71
                    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_ERROR,
                                                               GTK_BUTTONS_CLOSE,
                                                               "Máy chỉ có quyền truy cập admin.");
                    gtk_window_set_title(GTK_WINDOW(dialog), "Lỗi đăng nhập");
                    gtk_dialog_run(GTK_DIALOG(dialog));
                    gtk_widget_destroy(dialog);
                    login_success = FALSE;
                } else {
                    // Đăng nhập admin trên máy 71
                    gtk_widget_hide(login_window);
                    admin_window = create_admin_window();
                    gtk_widget_show_all(admin_window);
                }
            } else {
                // Các máy không hợp lệ
                gtk_widget_show_all(login_window);
                return;
            }

            if (login_success) {
                gtk_widget_hide(login_window);
            }

            break;
        }
    }

    if (!login_success) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(login_window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Sai tên đăng nhập hoặc mật khẩu.");
        gtk_window_set_title(GTK_WINDOW(dialog), "Lỗi đăng nhập");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        gtk_entry_set_text(GTK_ENTRY(((GtkWidget **)data)[0]),"");
        gtk_entry_set_text(GTK_ENTRY(((GtkWidget **)data)[1]),"");
    }
}


static gboolean on_username_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        gtk_widget_grab_focus(GTK_WIDGET(data));
        return TRUE;
    }
    return FALSE;
}

static gboolean on_password_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        on_login_button_clicked(widget, data);
        return TRUE;
    }
    return FALSE;
}

// Hàm main
int main(int argc, char *argv[]) {
    // Khởi tạo GTK
    gtk_init(&argc, &argv);

    // Lấy dữ liệu về đồ ăn uống
    getData();

    // Tạo cửa sổ đăng nhập
    login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_window), "Đăng nhập");
    gtk_window_set_default_size(GTK_WINDOW(login_window), 1920 , 1080);
    gtk_window_set_position(GTK_WINDOW(login_window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(login_window), FALSE); // Không cho phép thay đổi kích thước

    // Kết nối tín hiệu destroy để đóng ứng dụng
    g_signal_connect(login_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Tạo hộp căn chỉnh để chứa các mục nhập và nút
    GtkWidget *align = gtk_alignment_new(0.5, 0.5, 0, 0); // Căn giữa các widget
    gtk_container_add(GTK_CONTAINER(login_window), align);

    // Tạo hộp dọc để chứa các mục nhập và nút
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    // Tạo hộp chứa phụ để canh giữa các widget
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), center_box, TRUE, TRUE, 0);

    // Tạo nhãn username và ô nhập liệu
    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_box_pack_start(GTK_BOX(center_box), username_label, FALSE, FALSE, 0);

    username_entry = gtk_entry_new();
    gtk_widget_set_size_request(username_entry, 200, 30); // Thiết lập kích thước cho ô nhập liệu username
    gtk_box_pack_start(GTK_BOX(center_box), username_entry, FALSE, FALSE, 0);

    // Tạo nhãn password và ô nhập liệu
    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_box_pack_start(GTK_BOX(center_box), password_label, FALSE, FALSE, 0);

    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);  // Đặt chế độ ẩn cho ô password
    gtk_widget_set_size_request(password_entry, 200, 30); // Thiết lập kích thước cho ô nhập liệu password
    gtk_box_pack_start(GTK_BOX(center_box), password_entry, FALSE, FALSE, 0);

    // Tạo nhãn lỗi
    GtkWidget *error_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(center_box), error_label, FALSE, FALSE, 0);

    // Tạo khoảng trống trước nút đăng nhập
    GtkWidget *space = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(center_box), space, FALSE, FALSE, 10);

    // Tạo nút đăng nhập
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    gtk_box_pack_start(GTK_BOX(center_box), login_button, FALSE, FALSE, 0);

    // Kết nối các tín hiệu
    GtkWidget *entries[3] = {username_entry, password_entry, error_label};
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), entries);
    g_signal_connect(username_entry, "key-press-event", G_CALLBACK(on_username_entry_key_press), password_entry);
    g_signal_connect(password_entry, "key-press-event", G_CALLBACK(on_password_entry_key_press), entries);

    // Hiển thị tất cả các widget trong cửa sổ đăng nhập
    gtk_widget_show_all(login_window);

    // Bắt đầu vòng lặp chính của GTK
    gtk_main();

    return 0;
}