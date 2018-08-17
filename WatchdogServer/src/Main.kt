import com.fasterxml.jackson.databind.ObjectMapper
import com.fasterxml.jackson.dataformat.yaml.YAMLFactory
import kotlinx.coroutines.experimental.launch
import kotlinx.coroutines.experimental.runBlocking
import kotlinx.coroutines.experimental.sync.Mutex
import kotlinx.coroutines.experimental.sync.withLock
import java.io.File
import java.net.DatagramPacket
import java.net.DatagramSocket

var timeToSend = "0000"
var timeToSendChanged = false
val timeToSendMutex = Mutex()

fun main(args: Array<String>) {
    System.out.println("hello")
    val mapper = ObjectMapper(YAMLFactory())
    val config: Config
    try {
        config = mapper.readValue(File("config.yaml"), Config::class.java)
    } catch (e: Exception) {
        println("config not found")
        e.printStackTrace()
        return
    }

    launch {
        listenForClient(DatagramSocket(config.clientPort))
    }
    runBlocking {
        listenForController(controllerSocket = DatagramSocket(config.controllerPort))
    }
}

suspend fun listenForController(controllerSocket: DatagramSocket) {
    val buf = ByteArray(16)
    val packet = DatagramPacket(buf, 16)
    while(true) {
        controllerSocket.receive(packet)
        println("received: ${String(packet.data)}")
        val controllerMsg = packet.data.copyOfRange(0,1)
        val responseMsg = constructControllerResponse(controllerMsg[0]).toByteArray()
    try {
            controllerSocket.send(DatagramPacket(responseMsg, responseMsg.size, packet.address, packet.port))
        } catch (e: Exception) {
            println("couldn't send controller response")
            continue
        }
        println("sent answer to controller: ${String(responseMsg)}")
    }
}

suspend fun constructControllerResponse(controllerMessage: Byte): String {
    timeToSendMutex.withLock {
        if (timeToSendChanged || controllerMessage.equals(0)) { // send time when time changed or if it's the controllers init message
            timeToSendChanged = false
            return "1$timeToSend" // first digit indicates that time changed since last update
        }
        return "0" // indicates that the time didn't change since the last update
    }
}

/*fun convertMsgToByteArray(msg: String) : ByteArray {
    val msgInt : Int
    try {
        msgInt = msg.toInt()
    } catch (e: NumberFormatException) {

    }
}*/

suspend fun listenForClient(clientSocket: DatagramSocket) {
    val buf = ByteArray(16)
    val packet = DatagramPacket(buf, 16)
    while(true) {
        clientSocket.receive(packet)
        val receivedString = String(packet.data.copyOfRange(0,4))
        println("received message: $receivedString")
        val msg = messageFormatIsValid(receivedString)
        if(msg == null) {
            println("malformed message")
            continue
        }
        if(msg!=timeToSend) {
            timeToSendMutex.withLock {
                timeToSend = msg
                timeToSendChanged = (true)
                println(timeToSend)
            }
        }
    }
}

/**
 * returns null when message format is invalid, otherwise returns the valid message
 */
fun messageFormatIsValid(msg: String): String? {
    val hour: Int
    val minute: Int
    var returnMsg = msg
    try {
        hour = msg.substring(0..1).toInt()
        if(hour == 0) // otherwise -0 would be a valid hour
            returnMsg = returnMsg.replaceRange(0..1, "00")
        minute = msg.substring(2..3).toInt()
        if(minute == 0)
            returnMsg = returnMsg.replaceRange(2..3, "00")
    } catch (e: NumberFormatException) {
        return null
    }
    return if(hour in 0..23 && (minute in 0..59)) {
        returnMsg
    }
    else
        null
}