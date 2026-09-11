package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"
)

type Room struct {
	Code         string    `json:"code"`
	HostIP       string    `json:"host_ip"`
	HostPort     int       `json:"host_port"`
	ClientIP     string    `json:"client_ip"`
	ClientPort   int       `json:"client_port"`
	ClientReady  bool      `json:"client_ready"`
	CreatedAt    time.Time `json:"created_at"`
	LastActivity time.Time `json:"last_activity"`
}

type CreateRoomRequest struct {
	Code     string `json:"code"`
	HostIP   string `json:"host_ip"`
	HostPort int    `json:"host_port"`
}

type JoinRoomRequest struct {
	Code       string `json:"code"`
	ClientIP   string `json:"client_ip"`
	ClientPort int    `json:"client_port"`
}

var (
	roomsLock sync.Mutex
	rooms     = make(map[string]*Room)
)

func cleanupOldRooms() {
	for {
		time.Sleep(1 * time.Minute)
		roomsLock.Lock()
		now := time.Now()
		for code, room := range rooms {
			// Expire rooms inactive for > 15 minutes
			if now.Sub(room.LastActivity) > 15*time.Minute {
				delete(rooms, code)
				log.Printf("[Signaling] Room %s expired and cleaned up", code)
			}
		}
		roomsLock.Unlock()
	}
}

func handleCreateRoom(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req CreateRoomRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.Code == "" {
		http.Error(w, "Invalid request payload", http.StatusBadRequest)
		return
	}

	roomsLock.Lock()
	defer roomsLock.Unlock()

	room := &Room{
		Code:         req.Code,
		HostIP:       req.HostIP,
		HostPort:     req.HostPort,
		CreatedAt:    time.Now(),
		LastActivity: time.Now(),
	}
	rooms[req.Code] = room

	log.Printf("[Signaling] Room created: %s -> Host %s:%d", req.Code, req.HostIP, req.HostPort)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"success": true,
		"room":    req.Code,
	})
}

func handleJoinRoom(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req JoinRoomRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.Code == "" {
		http.Error(w, "Invalid request payload", http.StatusBadRequest)
		return
	}

	roomsLock.Lock()
	defer roomsLock.Unlock()

	room, exists := rooms[req.Code]
	if !exists {
		http.Error(w, "Room not found", http.StatusNotFound)
		return
	}

	room.ClientIP = req.ClientIP
	room.ClientPort = req.ClientPort
	room.ClientReady = true
	room.LastActivity = time.Now()

	log.Printf("[Signaling] Peer joined Room %s -> Client %s:%d (Host is %s:%d)",
		req.Code, req.ClientIP, req.ClientPort, room.HostIP, room.HostPort)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"success":   true,
		"room":      req.Code,
		"host_ip":   room.HostIP,
		"host_port": room.HostPort,
	})
}

func handlePollRoom(w http.ResponseWriter, r *http.Request) {
	code := r.URL.Query().Get("room")
	if code == "" {
		http.Error(w, "Missing room query parameter", http.StatusBadRequest)
		return
	}

	roomsLock.Lock()
	defer roomsLock.Unlock()

	room, exists := rooms[code]
	if !exists {
		http.Error(w, "Room not found", http.StatusNotFound)
		return
	}

	room.LastActivity = time.Now()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"success":      true,
		"room":         room.Code,
		"client_ready": room.ClientReady,
		"client_ip":    room.ClientIP,
		"client_port":  room.ClientPort,
	})
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	go cleanupOldRooms()

	http.HandleFunc("/api/create_room", handleCreateRoom)
	http.HandleFunc("/api/join_room", handleJoinRoom)
	http.HandleFunc("/api/poll_room", handlePollRoom)
	http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		fmt.Fprintf(w, "OK")
	})

	log.Printf("Chained Decos Signaling Server running on port %s...", port)
	if err := http.ListenAndServe(":"+port, nil); err != nil {
		log.Fatalf("Server failed: %v", err)
	}
}
