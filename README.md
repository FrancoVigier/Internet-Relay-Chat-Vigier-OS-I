# Internet-Relay-Chat-Vigier-OS-I
IRC (Internet Relay Chat). Server in Erlang and Client in C

Internet Relay Chat (IRC) is a text-based chat system for instant messaging.
IRC is designed for group communication in discussion forums, called channels, 
but also allows one-on-one communication via private messages

un cliente que envía 4 tipos de mensajes:
- /msg [nickname] [msg] : envía el mensaje al servidor que luego lo redirecciona al cliente cuyo nickname coincida con el enviado acá
- /exit : envía un mensaje al servidor que remueve al usuario de la lista de usuarios logueados
- /nickname [nickname]: envía un mensaje al servidor para que modifique el nickname del usuario y le notifique de este cambio
- mensaje genérico : envía un mensaje al servidor que luego lo redirecciona a todos los clientes excepto al que mandó el mensaje
