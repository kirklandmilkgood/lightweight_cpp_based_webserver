# lightweight_cpp_based_webserver
本專案以c++為主體，分成多個功能區塊，建立一輕量級的網頁伺服器，本專案主要目的為熟悉linux下網路程式的編寫與提升c++程式編寫能力
此專案依照功能可分為以下部分：
1. 紀錄檔系統 (建立一個threadpool，在multithread環境下進行紀錄檔的寫入任務，提高運作效率)
2. 資料庫(mysql)connection pool (以linked-list為基礎管理資料庫的連接，以完成登錄時用戶名稱與密碼的校驗)
3. http連接管理(將http連接抽象為一個類別，每筆連接都是一個 http connection物件，以此物件搭配threadpool的機制，能在高效率下完成網路IO操作，處理同時間段內多筆連接的資源請求)
4. 同步機制類別的封裝(將c語言內建的pthread_mutex, pthread_cond等同步機制封裝成新的類別，方便呼叫，在multithreads的環境下能充分保證共用資源的一致性)
5. 定時器系統(由於client與server端建立連接後，可能出現長時間不傳輸數據的情況，白白佔用資源，因此為每筆連接設定定時器，若超過設定時間未有數據傳輸，則系統自動斷開連接)
6. threadpool (pool為一組資源的集合，threadpool即是一組thread的集合，在本專案中無論是連接的處理、IO操作、紀錄檔的寫入等都利用此概念進行，當server處理完一個任務後，可以將
對應的thread放回pool中，在之後同樣的thread仍可用於別的任務，既保證了程式的高效也不造成過多的資源浪費)
