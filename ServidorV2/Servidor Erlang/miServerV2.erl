-module(miServerV2).
-export([start/1]).
-export([manejarConexiones/2, manejarUsuarios/1, recibirMensajes/2]).

start(Port) ->
    {ok, LSock} = gen_tcp:listen(Port, [list, {active, false}]), %Escuchamos el puerto y te devuelve el socket por donde se van a conectar los usuarios LSock(Inicializamos el socket)
    spawn(?MODULE, manejarConexiones, [LSock, 1]), %Una vez inicializado el LSock para escuchar. Creamos un actor/proceso concurrente. Que basicamente va a aceptar todo intento de conexión de los clientes
    register(usuarios, spawn(?MODULE, manejarUsuarios, [maps:new()])), %Generamos un proceso para la gestion de todos los usuarios que están en un map del tipo[(Socket, Nombre)]
    ok.

manejarConexiones(LSock, Id) ->
    case gen_tcp:accept(LSock) of  %Vamos a aceptar a todo Cliente que quiera unirse a la sala de chat
        {ok, Socket} ->  % Si la conexion es exitosa, Creamos un Misterioso 10
            Nombre = "Misterioso" ++ integer_to_list(Id),
            usuarios ! {agregar, Nombre, Socket},  %Al proceso usuarios le mandamos el Misterioso X y su socket para que lo agregue al Mapa donde están todos
            spawn(?MODULE, recibirMensajes, [Socket, Nombre]); %A cada Cliente le vamos a asignar un Actor. Por el cual vamos a "Escribir" o mandarle cosas a nuestro socket. Con RECV
        {error, _} -> io:format("Error")
    end,
    manejarConexiones(LSock, Id + 1).

manejarUsuarios(Mapa) ->
    receive
        {agregar, Nombre, Socket} ->
            usuarios ! {broadcast, Nombre ++ " se unio a la sala" ++ [0]},
            manejarUsuarios(maps:put(Nombre, Socket, Mapa));
        {eliminar, Nombre} ->
            usuarios ! {broadcast, Nombre ++ " se fue de la sala" ++ [0]},
            manejarUsuarios(maps:remove(Nombre, Mapa));
        {mensaje, Nombre, Mensaje} ->
            gen_tcp:send(maps:get(Nombre, Mapa), Mensaje),
            manejarUsuarios(Mapa);
        {renombrar, Nombre, Nuevo} ->
            usuarios ! {broadcast, Nombre ++ " ahora es " ++ Nuevo ++ [0]},
            manejarUsuarios(maps:put(Nuevo, maps:get(Nombre, Mapa),
            maps:remove(Nombre, Mapa)));
        {broadcast, Mensaje} ->
            lists:map(fun(Socket) -> gen_tcp:send(Socket, Mensaje) end,
            maps:values(Mapa)),
            manejarUsuarios(Mapa);
        {existe, Nombre, Pid} ->
            Pid ! maps:is_key(Nombre, Mapa),
            manejarUsuarios(Mapa)
    end.

recibirMensajes(Socket, Nombre) -> %Proceso/Actor de escritura
    case gen_tcp:recv(Socket, 1024) of %Si el Socket de un cliente recibe algo. Es decir si el cliente escribe un mensaje.
        {ok, "/exit" ++ _} -> usuarios ! {eliminar, Nombre}; %Si mandamos un "/exit", queremos mandar un atomo al gestor de Usuarios para que se saque del MAP el usuario
        {ok, "/nickname " ++ Resto} -> %Si mandamos un "/nickname xxxx"
            Nuevo = hd(string:lexemes(Resto, " " ++ [$\n] ++ [0])), %Con lexema extraemos el nuevo nickname
            usuarios ! {existe, Nuevo, self()}, %Al gestor de usuarios vamos a mandar el Atomo "existe" que va a controlar si el nombre está o no
            receive
                true -> gen_tcp:send(Socket, "Nombre rechazado " ++ [0]), recibirMensajes(Socket, Nombre); %Si está el nombre lo rechazo
                false -> usuarios ! {renombrar, Nombre, Nuevo}, recibirMensajes(Socket, Nuevo) % Si NO está el nombre entonces le digo al gestor de Usuarios que lo renombre
            end;
        {ok, Paquete} -> 
            case Paquete of
                "/msg " ++ Resto -> %Si recibo un /msg saco el destino y mensaje
                    [Destino | Mensaje] = string:split(Resto, " "),
                    usuarios ! {mensaje, Destino, hd(string:lexemes("" ++ Nombre ++ "[Private]: " ++ Mensaje, [0])) ++ [0]};
                Mensaje -> usuarios ! {broadcast, hd(string:lexemes(Nombre ++ ": " ++ Mensaje, [0])) ++ [0]}
            end,
            recibirMensajes(Socket, Nombre);
        {error, _} -> io:format("Error inesperado")
    end.