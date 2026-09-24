#include "header.h"

// 우클릭 핸들러
void on_button_pressed(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    if (n_press == 1) {  // 단일 클릭인 경우
        GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
        // 우클릭인 경우에만 팝업 메뉴 표시
        if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) == GDK_BUTTON_SECONDARY) {
            show_popup_menu(widget, x, y, user_data);
        }
    }
}

void show_popup_menu(GtkWidget *parent_widget, double x, double y, gpointer user_data) {
    UserData *data = user_data;
    g_print("Showing popup menu at (%f, %f)\n", x, y);

    // GMenu 생성
    GMenu *gmenu = g_menu_new();

    if (data->selected_file_info) {
        // 파일/폴더가 선택된 경우에만 표시할 메뉴
        g_menu_append(gmenu, "이름 변경", "app.rename");
        g_menu_append(gmenu, "삭제", "app.delete");
		g_menu_append(gmenu, "이동", "app.move");
		g_menu_append(gmenu, "복사", "app.copy");
		g_menu_append(gmenu, "권한 변경", "app.chmod");
		g_menu_append(gmenu, "즐겨찾기에 추가", "app.add_favorite");
    }
    // 항상 표시할 메뉴
    g_menu_append(gmenu, "새 디렉토리 만들기", "app.new_directory");

    // GtkPopoverMenu 생성
    GtkWidget *menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(gmenu));
    gtk_widget_set_parent(menu, parent_widget);  // 부모 위젯 설정

    // 위치 설정
    GdkRectangle rect = { .x = (int)x, .y = (int)y, .width = 1, .height = 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(menu), &rect);

    // 팝업 표시
    gtk_popover_popup(GTK_POPOVER(menu));
    g_object_unref(gmenu);
}


//  선택 변경 핸들러 
void on_selection_changed(GtkSingleSelection *single_sel, guint position, guint n_items, gpointer user_data) {
    UserData *data = (UserData *)user_data;

    // 이전 선택 정보 해제
    if (data->selected_file_info) {
        g_object_unref(data->selected_file_info);
        data->selected_file_info = NULL;
    }

    // 새로운 선택 정보 저장
	GtkSelectionModel *selection_model = GTK_SELECTION_MODEL(single_sel);
    GtkBitset *selected = gtk_selection_model_get_selection(selection_model);

    if (gtk_bitset_get_nth(selected, 0) != GTK_INVALID_LIST_POSITION) {
        GFileInfo *info = G_FILE_INFO(g_list_model_get_item(G_LIST_MODEL(selection_model),
                                    gtk_bitset_get_nth(selected, 0)));
        if (info) {
            data->selected_file_info = g_object_ref(info);
        }
    }
    gtk_bitset_unref(selected);
}

// 이름 변경 실행 함수
void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    UserData *data = user_data;
    if (response_id == GTK_RESPONSE_OK && data->selected_file_info) {
        const char *new_name = gtk_editable_get_text(GTK_EDITABLE(data->entry));
        const char *file_name = g_file_info_get_name(data->selected_file_info);
        const char *current_path = gtk_editable_get_text(GTK_EDITABLE(data->path_entry));

        // 입력값 검증
        if (!new_name || !*new_name) {
            g_printerr("New name is empty.\n");
            gtk_window_close(GTK_WINDOW(dialog));
            return;
        }
        if (!current_path || !*current_path) {
            g_printerr("Current path is empty.\n");
            gtk_window_close(GTK_WINDOW(dialog));
            return;
        }

        // 이전 파일의 전체 경로
        char *old_path = g_build_filename(current_path, file_name, NULL);
        // 새 파일의 전체 경로
        char *new_path = g_build_filename(current_path, new_name, NULL);

        // 메시지 구성
        char *message = g_strdup_printf("%s$%s", old_path, new_path);
        g_printerr("message : %s\n", message);

        data->is_backend_done = NULL;
        send_message(4, message, NULL);

        g_free(message);
        g_free(old_path);
        g_free(new_path);
    }
    start_backend_check(data);
    gtk_window_close(GTK_WINDOW(dialog));
}


// 이름 변경 콜백 함수
void on_rename_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    UserData *data = user_data;
    if (!data->selected_file_info) {
        g_printerr("No file selected to rename.\n");
        return;
    }
    const char *current_name = g_file_info_get_name(data->selected_file_info);
	// Rename dialog 생성
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Rename File",
        GTK_WINDOW(data->main_window),
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename", GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 다이얼로그에 설명 라벨 추가
    GtkWidget *label = gtk_label_new("Enter new file name:");
    gtk_box_append(GTK_BOX(content_area), label);

    // 텍스트 입력 위젯 추가
    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), current_name); // 기존 파일 이름 설정
    gtk_box_append(GTK_BOX(content_area), entry);

	data->entry = GTK_ENTRY(entry);
    // 다이얼로그 표시
    gtk_widget_show(dialog);

    // 다이얼로그의 "response" 시그널 처리
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), data);
}

void on_delete_response(GtkDialog *dialog, int response, gpointer user_data) {
    UserData *data = user_data;
    if (response == GTK_RESPONSE_YES && data->selected_file_info) {

        // 수정 코드:
		const char *file_name = g_file_info_get_name(data->selected_file_info);
		const char *current_path = gtk_editable_get_text(GTK_EDITABLE(data->path_entry));
		char *file_path = g_build_filename(current_path, file_name, NULL);

		data->is_backend_done = NULL;
		// 백엔드로 메시지 전송
		send_message(2, file_path, dialog);
		g_free(file_path);
    }
	// g_object_unref(dialog);
    start_backend_check(data);
	gtk_window_close(GTK_WINDOW(dialog));
}


// 파일, 디렉토리 삭제 콜백함
void on_delete_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    UserData *data = user_data;
    if (!data->selected_file_info) {
        g_printerr("No file selected to delete.\n");
        return;
    }

    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(data->main_window),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_YES_NO,
                                  "Are you sure you want to delete?");

    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), data);
    gtk_widget_show(dialog);
}

/* 새 디렉토리 만들기 액션 콜백 */
void on_new_directory_action (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    UserData *data = user_data;
    const char *current_path = gtk_editable_get_text(GTK_EDITABLE(data->path_entry));
    char *new_folder_path = g_build_filename(current_path, "New Folder", NULL);

    GFile *new_dir = g_file_new_for_path(new_folder_path);
	const char *path = g_file_get_path(new_dir);

	// 메시지 전송
	send_message(3, path, NULL);

	// 디렉토리 재조회
    // GFile *cur_dir = g_file_new_for_path(current_path);
    // gtk_directory_list_set_file(data->directorylist, cur_dir);
    // g_object_unref(cur_dir);
	data->is_backend_done = NULL;
	start_backend_check(data);

    g_object_unref(new_dir);
    g_free(new_folder_path);
}

/* 애플리케이션 종료 시 메시지 큐 정리 */
void on_application_shutdown(GApplication *app, gpointer user_data) {
    struct mymsgbuf buf;
    key_t command_key, response_key;

    // command 메시지 큐 정리
    command_key = ftok("command_keyfile", 1);
    if (command_key != -1) {
        int command_msgid = msgget(command_key, 0644);
        if (command_msgid != -1) {
            buf.mtype = 1;
            strcpy(buf.mtext, "QUIT");
            msgsnd(command_msgid, &buf, strlen(buf.mtext) + 1, 0);
            usleep(100000);
            msgctl(command_msgid, IPC_RMID, NULL);
            g_print("Command message queue removed.\n");
        }
    }

    // response 메시지 큐 정리
    response_key = ftok("response_keyfile", 1);
    if (response_key != -1) {
        int response_msgid = msgget(response_key, 0644);
        if (response_msgid != -1) {
            msgctl(response_msgid, IPC_RMID, NULL);
            g_print("Response message queue removed.\n");
        }
    }
}

// 권한 부여 콜백 함수
void on_chmod_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    UserData *data = user_data;
    if (!data->selected_file_info) {
        g_printerr("No file selected for chmod.\n");
        return;
    }

    // 현재 파일의 경로 구성
    const char *file_name = g_file_info_get_name(data->selected_file_info);
    const char *current_path = gtk_editable_get_text(GTK_EDITABLE(data->path_entry));
    char *file_path = g_build_filename(current_path, file_name, NULL);

    // 현재 파일의 권한 가져오기
    struct stat st;
    if (stat(file_path, &st) != 0) {
        g_printerr("Failed to get file permissions.\n");
        g_free(file_path);
        return;
    }

    // 8진수로 권한 변환 (예: 644)
    mode_t mode = st.st_mode & 0777;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "권한 변경",
        GTK_WINDOW(data->main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
        "_취소", GTK_RESPONSE_CANCEL,
        "_변경", GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 10);
    gtk_widget_set_margin_bottom(vbox, 10);

    // 파일 정보 표시
    char *info_text = g_strdup_printf("선택된 파일: %s\n현재 권한: %03o", file_name, mode);
    GtkWidget *info_label = gtk_label_new(info_text);
    gtk_widget_set_halign(info_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), info_label);
    g_free(info_text);

    GtkWidget *perm_label = gtk_label_new("새 권한 (8진수, 예: 644):");
    gtk_widget_set_halign(perm_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), perm_label);

    GtkWidget *entry = gtk_entry_new();
    char mode_str[4];
    snprintf(mode_str, sizeof(mode_str), "%03o", mode);
    gtk_editable_set_text(GTK_EDITABLE(entry), mode_str);
    gtk_box_append(GTK_BOX(vbox), entry);

    gtk_box_append(GTK_BOX(content_area), vbox);
    data->entry = GTK_ENTRY(entry);

    gtk_widget_show(dialog);

    // 경로 저장을 위한 데이터 전달
    g_object_set_data_full(G_OBJECT(dialog), "file_path", file_path, g_free);
    g_signal_connect(dialog, "response", G_CALLBACK(on_chmod_dialog_response), data);
}

// 사용자 제시 권한 부여하기
void on_chmod_dialog_response(GtkDialog *dialog, int response, gpointer user_data) {
    UserData *data = user_data;

    if (response == GTK_RESPONSE_OK) {
        const char *mode_str = gtk_editable_get_text(GTK_EDITABLE(data->entry));
        char *file_path = g_object_get_data(G_OBJECT(dialog), "file_path");

        // 권한 문자열 유효성 검사
        char *endptr;
        long mode = strtol(mode_str, &endptr, 8);
        if (*endptr != '\0' || mode < 0 || mode > 0777) {
            g_printerr("Invalid permission format. Use octal numbers (0-7).\n");
            gtk_window_close(GTK_WINDOW(dialog));
            return;
        }

        char *message = g_strdup_printf("%s$%s", file_path, mode_str);
        data->is_backend_done = FALSE;
        send_message(7, message, dialog);  // 메시지 타입 7은 chmod 작업
        g_free(message);
    }

    gtk_window_close(GTK_WINDOW(dialog));
    start_backend_check(data);
}



