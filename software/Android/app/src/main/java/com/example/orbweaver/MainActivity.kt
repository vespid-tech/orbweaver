package com.example.orbweaver

import android.Manifest
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.os.Build
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import androidx.lifecycle.lifecycleScope
import com.example.orbweaver.ui.theme.OrbweaverTheme
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority
import com.google.android.gms.tasks.CancellationTokenSource
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.*

class MainActivity : ComponentActivity(), SerialInputOutputManager.Listener, SensorEventListener {
    private val ACTION_USB_PERMISSION = "com.example.orbweaver.USB_PERMISSION"
    
    private var usbSerialPort: UsbSerialPort? = null
    private var usbIoManager: SerialInputOutputManager? = null
    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var sensorManager: SensorManager
    private var pressureSensor: Sensor? = null
    private var currentPressure: Float = 0f
    
    private var historyState: MutableState<AnnotatedString> = mutableStateOf(AnnotatedString(""))
    private var isConnectedState = mutableStateOf(false)
    private var sourceAddressState = mutableStateOf("254")
    private var destinationAddressState = mutableStateOf("255")
    private var localDeviceState = mutableStateOf("None")
    private var bitRateState = mutableStateOf("50 kbps")
    private var channelState = mutableStateOf("26")
    
    private var isEchoPending = false
    private val rxBuffer = mutableListOf<Byte>()

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (ACTION_USB_PERMISSION == intent.action) {
                synchronized(this) {
                    val device: UsbDevice? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    }
                    
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        device?.let { connectToDevice(it) }
                    } else {
                        updateHistory("Permission denied for device $device")
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)
        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        pressureSensor = sensorManager.getDefaultSensor(Sensor.TYPE_PRESSURE)
        
        val filter = IntentFilter(ACTION_USB_PERMISSION)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(usbReceiver, filter)
        }

        setContent {
            OrbweaverTheme {
                MainScreen(
                    history = historyState.value,
                    isConnected = isConnectedState.value,
                    sourceAddress = sourceAddressState.value,
                    destinationAddress = destinationAddressState.value,
                    localDevice = localDeviceState.value,
                    selectedBitRate = bitRateState.value,
                    selectedChannel = channelState.value,
                    onSourceAddressChange = { onSourceAddressUpdated(it) },
                    onDestinationAddressChange = { destinationAddressState.value = it },
                    onBitRateChange = { onBitRateUpdated(it) },
                    onChannelChange = { onChannelUpdated(it) },
                    onEchoClick = { onEchoPressed() },
                    onGetRangeClick = { onGetRangePressed() },
                    onConnectClick = { discoverAndRequest() },
                    onSendMessage = { msg -> sendTextMessage(msg) }
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()
        pressureSensor?.let {
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_NORMAL)
        }
    }

    override fun onPause() {
        super.onPause()
        sensorManager.unregisterListener(this)
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(usbReceiver)
        disconnect()
    }

    private fun onSourceAddressUpdated(newAddr: String) {
        sourceAddressState.value = newAddr
        val src = newAddr.toIntOrNull()?.toByte() ?: return
        
        if (isConnectedState.value) {
            val localAddr = localDeviceState.value.toIntOrNull()?.toByte() ?: 0xFF.toByte()
            val header = SystemCmdHeader(
                aad = Aad(sequence = 0x00, addrSrc = src, addrDest = localAddr),
                ttl = 0x00,
                length = 7,
                command = SystemCmd.UART_PEERS_CMD
            )
            sendPacket(header, byteArrayOf(src))
        }
    }

    private fun onBitRateUpdated(newBitRate: String) {
        bitRateState.value = newBitRate
        val mapping = mapOf(
            "625 bps" to 1.toByte(),
            "2.5 kbps" to 2.toByte(),
            "5 kbps" to 3.toByte(),
            "50 kbps" to 4.toByte(),
            "200 kbps" to 5.toByte(),
            "300 kbps" to 6.toByte(),
            "400 kbps" to 7.toByte(),
            "500 kbps" to 8.toByte()
        )
        val bitRateValue = mapping[newBitRate] ?: return

        if (isConnectedState.value) {
            val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
            val localAddr = localDeviceState.value.toIntOrNull()?.toByte() ?: 0xFF.toByte()
            val header = SystemCmdHeader(
                aad = Aad(sequence = 0x00, addrSrc = src, addrDest = localAddr),
                ttl = 0x00,
                length = 7,
                command = SystemCmd.BITRATE_SET_CMD
            )
            sendPacket(header, byteArrayOf(bitRateValue))
        }
    }

    private fun onChannelUpdated(newChannel: String) {
        channelState.value = newChannel
        val channelValue = newChannel.toIntOrNull()?.toByte() ?: return

        if (isConnectedState.value) {
            val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
            val localAddr = localDeviceState.value.toIntOrNull()?.toByte() ?: 0xFF.toByte()
            val header = SystemCmdHeader(
                aad = Aad(sequence = 0x00, addrSrc = src, addrDest = localAddr),
                ttl = 0x00,
                length = 7,
                command = SystemCmd.CHANNEL_SET_CMD
            )
            sendPacket(header, byteArrayOf(channelValue))
        }
    }

    private fun onEchoPressed() {
        if (!isConnectedState.value) return
        
        val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
        val dest = destinationAddressState.value.toIntOrNull()?.toByte() ?: 0xFF.toByte()
        
        isEchoPending = true
        val header = SystemCmdHeader(
            aad = Aad(sequence = 0x00, addrSrc = src, addrDest = dest),
            ttl = 0x00,
            length = 6,
            command = SystemCmd.ECHO_CMD
        )
        sendPacket(header)
    }

    private fun onGetRangePressed() {
        if (!isConnectedState.value) return
        
        val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
        val dest = destinationAddressState.value.toIntOrNull()?.toByte() ?: 0xFF.toByte()
        
        updateHistory("Requesting range from ${dest.toInt() and 0xFF}...")
        val header = SystemCmdHeader(
            aad = Aad(sequence = 0x00, addrSrc = src, addrDest = dest),
            ttl = 0x00,
            length = 7,
            command = SystemCmd.BLOB_CMD
        )
        sendPacket(header, byteArrayOf(0x02.toByte()))
    }

    private fun discoverAndRequest() {
        if (isConnectedState.value) {
            disconnect()
            return
        }

        val manager = getSystemService(Context.USB_SERVICE) as UsbManager
        val availableDrivers = UsbSerialProber.getDefaultProber().findAllDrivers(manager)
        if (availableDrivers.isEmpty()) {
            updateHistory("No USB serial devices found.")
            return
        }

        val driver = availableDrivers[0]
        val device = driver.device

        if (manager.hasPermission(device)) {
            connectToDevice(device)
        } else {
            val flags = PendingIntent.FLAG_MUTABLE
            val intent = Intent(ACTION_USB_PERMISSION).apply {
                setPackage(packageName)
            }
            val permissionIntent = PendingIntent.getBroadcast(this, 0, intent, flags)
            manager.requestPermission(device, permissionIntent)
        }
    }

    private fun connectToDevice(device: UsbDevice) {
        val manager = getSystemService(Context.USB_SERVICE) as UsbManager
        val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
        val connection = manager.openDevice(driver.device)
        if (connection == null) {
            updateHistory("Failed to open USB connection.")
            return
        }

        val port = driver.ports[0]
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                port.open(connection)
                port.setParameters(460800, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
                
                // Set DTR/RTS signals as many devices require this to start sending data
                try {
                    port.dtr = true
                    port.rts = true
                } catch (e: Exception) {
                    // some devices don't support this, ignore
                }

                usbSerialPort = port
                
                usbIoManager = SerialInputOutputManager(port, this@MainActivity)
                usbIoManager?.start()
                
                withContext(Dispatchers.Main) {
                    updateHistory("Connected to ${device.deviceName}, initializing handshake...")
                }
                
                // Send initial packets
                val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
                
                val header1 = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = src, addrDest = 0xFF.toByte()),
                    ttl = 0x00,
                    length = 7,
                    command = SystemCmd.UART_PEERS_CMD
                )
                sendPacket(header1, byteArrayOf(src))
                
                val header2 = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = src, addrDest = 0xFF.toByte()),
                    ttl = 0x00,
                    length = 6,
                    command = SystemCmd.ECHO_CMD
                )
                sendPacket(header2)
                
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    updateHistory("Error opening port: ${e.message}")
                    disconnect()
                }
            }
        }
    }

    private fun sendTextMessage(msg: String) {
        val trimmedMsg = msg.trim()
        val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
        val dest = destinationAddressState.value.toIntOrNull()?.toByte() ?: 0
        val msgBytes = trimmedMsg.toByteArray()
        val payload = ByteArray(msgBytes.size + 1)
        payload[0] = 0x01.toByte()
        System.arraycopy(msgBytes, 0, payload, 1, msgBytes.size)
        
        updateHistory("Me: $trimmedMsg")
        
        val header = SystemCmdHeader(
            aad = Aad(sequence = 0x00, addrSrc = src, addrDest = dest),
            ttl = 0x00,
            length = (6 + payload.size).toByte(),
            command = SystemCmd.BLOB_CMD
        )
        sendPacket(header, payload)
    }

    private fun sendPacket(header: SystemCmdHeader, payload: ByteArray = byteArrayOf()) {
        val port = usbSerialPort ?: return
        val headerBytes = header.toByteArray()
        val totalSize = 1 + headerBytes.size + payload.size + 1
        val packet = ByteArray(totalSize)
        
        packet[0] = 0xAA.toByte() // Start byte
        System.arraycopy(headerBytes, 0, packet, 1, headerBytes.size)
        if (payload.isNotEmpty()) {
            System.arraycopy(payload, 0, packet, 1 + headerBytes.size, payload.size)
        }
        packet[totalSize - 1] = 0xCC.toByte() // Stop byte
        
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                port.write(packet, 2000)
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    updateHistory("Send error: ${e.message}")
                }
            }
        }
    }

    override fun onNewData(data: ByteArray?) {
        data?.let { bytes ->
            if (bytes.isNotEmpty()) {
                lifecycleScope.launch(Dispatchers.Main) {
                    rxBuffer.addAll(bytes.toList())
                    processPackets()
                }
            }
        }
    }

    private fun processPackets() {
        val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
        
        while (rxBuffer.size >= 8) { // Minimum packet size is AA + Header(6) + CC = 8
            val startIndex = rxBuffer.indexOf(0xAA.toByte())
            if (startIndex == -1) {
                rxBuffer.clear()
                return
            }
            
            // Remove junk before 0xAA
            if (startIndex > 0) {
                repeat(startIndex) { rxBuffer.removeAt(0) }
            }
            
            if (rxBuffer.size < 7) return // Need at least AA + Header
            
            val length = rxBuffer[5].toInt() and 0xFF // length is at index 5 in the framed packet (aad(3) + ttl(1) + len(1) at index 5)
            // Wait, let's look at SystemCmdHeader: aad(3), ttl(1), length(1), command(1)
            // In packet: AA(0), aad.seq(1), aad.src(2), aad.dest(3), ttl(4), len(5), cmd(6) ... CC
            
            val totalPacketSize = length + 2
            if (rxBuffer.size < totalPacketSize) return // Need more data for full packet
            
            if (rxBuffer[totalPacketSize - 1] == 0xCC.toByte()) {
                // Valid framing!
                val headerBytes = rxBuffer.subList(1, 7).toByteArray()
                val header = SystemCmdHeader.fromByteArray(headerBytes)
                val payload = rxBuffer.subList(7, totalPacketSize - 1).toByteArray()
                
                handlePacket(header, payload, src)
                
                repeat(totalPacketSize) { rxBuffer.removeAt(0) }
            } else {
                // Invalid stop byte, discard this AA and keep looking
                rxBuffer.removeAt(0)
            }
        }
    }

    private fun handlePacket(header: SystemCmdHeader, payload: ByteArray, currentSrc: Byte) {
        if (!isConnectedState.value) {
            // Handshake logic
            if (header.command == SystemCmd.ECHO_RESPONSE_CMD && header.aad.addrDest == currentSrc) {
                isConnectedState.value = true
                val localAddr = header.aad.addrSrc
                localDeviceState.value = (localAddr.toInt() and 0xFF).toString()
                updateHistory("Handshake successful! Local device: ${localDeviceState.value}")
                
                // Fetch Bitrate and Channel from device
                val bitrateGetHeader = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = currentSrc, addrDest = localAddr),
                    ttl = 0x00,
                    length = 6,
                    command = SystemCmd.BITRATE_GET_CMD
                )
                sendPacket(bitrateGetHeader)
                
                val channelGetHeader = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = currentSrc, addrDest = localAddr),
                    ttl = 0x00,
                    length = 6,
                    command = SystemCmd.CHANNEL_GET_CMD
                )
                sendPacket(channelGetHeader)
            }
        } else {
            // Normal operation
            if (header.command == SystemCmd.BLOB_CMD && header.aad.addrDest == currentSrc) {
                if (payload.isNotEmpty()) {
                    val type = payload[0].toInt() and 0xFF
                    if (type == 0x01) {
                        val receivedMsg = String(payload, 1, payload.size - 1)
                        updateHistory("Them: $receivedMsg")
                    } else if (type == 0x02) {
                        updateHistory("Location request received")
                        requestLocationAndRespond(header.aad.addrSrc)
                    } else if (type == 0x03) {
                        val receivedMsg = String(payload, 1, payload.size - 1)
                        updateHistory("Location Data from ${header.aad.addrSrc.toInt() and 0xFF}: $receivedMsg")
                        calculateRange(header.aad.addrSrc, receivedMsg)
                    }
                }
                
                // Request RSSI from the local device
                val localAddr = localDeviceState.value.toIntOrNull()?.toByte() ?: 0
                val rssiHeader = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = currentSrc, addrDest = localAddr),
                    ttl = 0x00,
                    length = 6,
                    command = SystemCmd.RSSI_GET_CMD
                )
                sendPacket(rssiHeader)
            } else if (header.command == SystemCmd.ECHO_CMD && header.aad.addrDest == currentSrc) {
                updateHistory("Echo received")
                val responseHeader = SystemCmdHeader(
                    aad = Aad(sequence = 0x00, addrSrc = currentSrc, addrDest = header.aad.addrSrc),
                    ttl = 0x00,
                    length = (6 + payload.size).toByte(),
                    command = SystemCmd.ECHO_RESPONSE_CMD
                )
                sendPacket(responseHeader, payload)
            } else if (header.command == SystemCmd.ECHO_RESPONSE_CMD && header.aad.addrDest == currentSrc) {
                if (isEchoPending) {
                    val localAddr = localDeviceState.value.toIntOrNull()?.toByte() ?: 0
                    val rssiHeader = SystemCmdHeader(
                        aad = Aad(sequence = 0x00, addrSrc = currentSrc, addrDest = localAddr),
                        ttl = 0x00,
                        length = 6,
                        command = SystemCmd.RSSI_GET_CMD
                    )
                    sendPacket(rssiHeader)
                }
            } else if (header.command == SystemCmd.RSSI_CMD && header.aad.addrDest == currentSrc) {
                if (payload.isNotEmpty()) {
                    val rssiValue = payload[0].toInt()
                    if (isEchoPending) {
                        updateHistory("Echo: $rssiValue")
                        isEchoPending = false
                    } else {
                        updateHistory("RSSI: $rssiValue")
                    }
                }
            } else if (header.command == SystemCmd.BITRATE_CMD && header.aad.addrDest == currentSrc) {
                if (payload.isNotEmpty()) {
                    val bitRateOptions = listOf("625 bps", "2.5 kbps", "5 kbps", "50 kbps", "200 kbps", "300 kbps", "400 kbps", "500 kbps")
                    val idx = (payload[0].toInt() and 0xFF) - 1
                    if (idx in bitRateOptions.indices) {
                        bitRateState.value = bitRateOptions[idx]
                        updateHistory("Device Bitrate: ${bitRateState.value}")
                    }
                }
            } else if (header.command == SystemCmd.CHANNEL_CMD && header.aad.addrDest == currentSrc) {
                if (payload.isNotEmpty()) {
                    val channel = (payload[0].toInt() and 0xFF).toString()
                    channelState.value = channel
                    updateHistory("Device Channel: $channel")
                }
            }
        }
    }

    override fun onRunError(e: Exception?) {
        runOnUiThread {
            updateHistory("USB Error: ${e?.message}")
            disconnect()
        }
    }

    private fun disconnect() {
        usbIoManager?.stop()
        usbIoManager = null
        try {
            usbSerialPort?.close()
        } catch (e: Exception) {
            // ignore
        }
        usbSerialPort = null
        isConnectedState.value = false
        localDeviceState.value = "None"
        bitRateState.value = "50 kbps"
        channelState.value = "26"
        rxBuffer.clear()
        updateHistory("Disconnected.")
    }

    private fun updateHistory(text: String) {
        val isMessage = text.startsWith("Me: ") || text.startsWith("Them: ")
        
        historyState.value = buildAnnotatedString {
            append(historyState.value)
            if (historyState.value.isNotEmpty()) {
                append("\n")
            }
            if (isMessage) {
                withStyle(style = SpanStyle(fontWeight = FontWeight.Bold)) {
                    append(text)
                }
            } else {
                append(text)
            }
        }
    }

    private fun requestLocationAndRespond(destAddr: Byte) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), 1001)
            updateHistory("Requesting location permission...")
            return
        }

        fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, CancellationTokenSource().token)
            .addOnSuccessListener { location: Location? ->
                if (location != null) {
                    lifecycleScope.launch(Dispatchers.IO) {
                        val gpsAlt = location.altitude
                        val baroAlt = if (currentPressure > 0) {
                            SensorManager.getAltitude(SensorManager.PRESSURE_STANDARD_ATMOSPHERE, currentPressure)
                        } else {
                            0.0
                        }
                        
                        val responseStr = "Lat: %.6f, Lon: %.6f, Alt(GPS): %.1fm, Alt(Baro): %.1fm".format(
                            location.latitude, location.longitude, gpsAlt, baroAlt
                        )
                        
                        val src = sourceAddressState.value.toIntOrNull()?.toByte() ?: 0
                        val msgBytes = responseStr.toByteArray()
                        val fullPayload = ByteArray(msgBytes.size + 1)
                        fullPayload[0] = 0x03.toByte()
                        System.arraycopy(msgBytes, 0, fullPayload, 1, msgBytes.size)

                        val header = SystemCmdHeader(
                            aad = Aad(sequence = 0x00, addrSrc = src, addrDest = destAddr),
                            ttl = 0x00,
                            length = (6 + fullPayload.size).toByte(),
                            command = SystemCmd.BLOB_CMD
                        )
                        sendPacket(header, fullPayload)
                        
                        withContext(Dispatchers.Main) {
                            updateHistory("Sent Location: $responseStr")
                        }
                    }
                } else {
                    updateHistory("Failed to get current location.")
                }
            }
    }

    private fun calculateRange(srcAddr: Byte, receivedStr: String) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            updateHistory("Missing location permission for range calculation.")
            return
        }

        // Parse received: "Lat: %.6f, Lon: %.6f, Alt(GPS): %.1fm, Alt(Baro): %.1fm"
        try {
            val regex = Regex("Lat: ([\\-\\d.]+), Lon: ([\\-\\d.]+), Alt\\(GPS\\): ([\\-\\d.]+)m, Alt\\(Baro\\): ([\\-\\d.]+)m")
            val match = regex.find(receivedStr) ?: return
            
            val remoteLat = match.groupValues[1].toDouble()
            val remoteLon = match.groupValues[2].toDouble()
            val remoteAlt = match.groupValues[3].toDouble() 

            fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, CancellationTokenSource().token)
                .addOnSuccessListener { location: Location? ->
                    if (location != null) {
                        val myLat = location.latitude
                        val myLon = location.longitude
                        val myAlt = location.altitude

                        // Flat surface approximation for < 20km
                        val latMid = (myLat + remoteLat) / 2.0 * (PI / 180.0)
                        val dy = (remoteLat - myLat) * 111320.0
                        val dx = (remoteLon - myLon) * 111320.0 * cos(latMid)
                        val dz = remoteAlt - myAlt
                        
                        val range = sqrt(dx * dx + dy * dy + dz * dz)
                        
                        updateHistory("Range to ${srcAddr.toInt() and 0xFF}: %.2f meters".format(range))
                    } else {
                        updateHistory("Range calc failed: My location unknown")
                    }
                }
        } catch (e: Exception) {
            Log.e("Protocol", "Error parsing location for range", e)
        }
    }

    override fun onSensorChanged(event: SensorEvent?) {
        if (event?.sensor?.type == Sensor.TYPE_PRESSURE) {
            currentPressure = event.values[0]
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    history: AnnotatedString,
    isConnected: Boolean,
    sourceAddress: String,
    destinationAddress: String,
    localDevice: String,
    selectedBitRate: String,
    selectedChannel: String,
    onSourceAddressChange: (String) -> Unit,
    onDestinationAddressChange: (String) -> Unit,
    onBitRateChange: (String) -> Unit,
    onChannelChange: (String) -> Unit,
    onEchoClick: () -> Unit,
    onGetRangeClick: () -> Unit,
    onConnectClick: () -> Unit,
    onSendMessage: (String) -> Unit
) {

    val bitRateOptions = listOf(
        "625 bps",
        "2.5 kbps",
        "5 kbps",
        "50 kbps",
        "200 kbps",
        "300 kbps",
        "400 kbps",
        "500 kbps"
    )
    var expanded by remember { mutableStateOf(false) }

    val channelOptions = (0..52).map { it.toString() }
    var channelExpanded by remember { mutableStateOf(false) }

    var message by remember { mutableStateOf("") }

    val vespidFont = FontFamily(
        Font(R.font.vespid, FontWeight.Normal)
    )

    fun validateAddress(input: String): String? {
        if (input.isEmpty()) return ""
        val value = input.toIntOrNull()
        return if (value != null && value in 1..255) input else null
    }

    Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .consumeWindowInsets(innerPadding)
                .imePadding()
                .padding(16.dp)
                .fillMaxSize()
        ) {
            Text(
                text = "ORBWEAVER",
                style = MaterialTheme.typography.headlineLarge.copy(
                    fontWeight = FontWeight.ExtraBold,
                    letterSpacing = 4.sp,
                    fontFamily = vespidFont
                ),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp),
                textAlign = TextAlign.Center,
                color = MaterialTheme.colorScheme.primary
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                OutlinedTextField(
                    value = sourceAddress,
                    onValueChange = { input ->
                        validateAddress(input)?.let { onSourceAddressChange(it) }
                    },
                    label = { Text("Source Address") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(16.dp))
                OutlinedTextField(
                    value = destinationAddress,
                    onValueChange = { input ->
                        validateAddress(input)?.let { onDestinationAddressChange(it) }
                    },
                    label = { Text("Destination Address") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(16.dp))
                OutlinedTextField(
                    value = localDevice,
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Local device Address") },
                    modifier = Modifier.weight(1f)
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                ExposedDropdownMenuBox(
                    expanded = expanded,
                    onExpandedChange = { expanded = !expanded },
                    modifier = Modifier.weight(1f)
                ) {
                    OutlinedTextField(
                        value = selectedBitRate,
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Bitrate") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                        colors = ExposedDropdownMenuDefaults.outlinedTextFieldColors(),
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable, true)
                    )
                    ExposedDropdownMenu(
                        expanded = expanded,
                        onDismissRequest = { expanded = false }
                    ) {
                        bitRateOptions.forEach { option ->
                            DropdownMenuItem(
                                text = { Text(option) },
                                onClick = {
                                    expanded = false
                                    onBitRateChange(option)
                                },
                                contentPadding = ExposedDropdownMenuDefaults.ItemContentPadding
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        ExposedDropdownMenuBox(
                            expanded = channelExpanded,
                            onExpandedChange = { channelExpanded = !channelExpanded },
                            modifier = Modifier.weight(1f)
                        ) {
                            OutlinedTextField(
                                value = selectedChannel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text("Channel") },
                                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = channelExpanded) },
                                colors = ExposedDropdownMenuDefaults.outlinedTextFieldColors(),
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable, true)
                            )
                            ExposedDropdownMenu(
                                expanded = channelExpanded,
                                onDismissRequest = { channelExpanded = false }
                            ) {
                                channelOptions.forEach { option ->
                                    DropdownMenuItem(
                                        text = { Text(option) },
                                        onClick = {
                                            channelExpanded = false
                                            onChannelChange(option)
                                        },
                                        contentPadding = ExposedDropdownMenuDefaults.ItemContentPadding
                                    )
                                }
                            }
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        val channelInt = selectedChannel.toInt()
                        val frequency = (channelInt / 2) + 902.0 + (if (channelInt % 2 != 0) 0.5 else 0.0)
                        Text(
                            text = "${"%.1f".format(frequency)} MHz",
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Button(
                    onClick = { onConnectClick() },
                    modifier = Modifier.weight(1f)
                ) {
                    Text(if (isConnected) "Disconnect" else "Connect")
                }
                Spacer(modifier = Modifier.width(16.dp))
                Surface(
                    color = if (isConnected) Color(0xFF4CAF50) else Color(0xFFF44336),
                    shape = MaterialTheme.shapes.small
                ) {
                    Text(
                        text = if (isConnected) "Connected" else "Not Connected",
                        color = Color.White,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Button(
                    onClick = onEchoClick,
                    enabled = isConnected,
                    modifier = Modifier.weight(1f)
                ) {
                    Text("Echo")
                }
                Spacer(modifier = Modifier.width(16.dp))
                Button(
                    onClick = onGetRangeClick,
                    enabled = isConnected,
                    modifier = Modifier.weight(1f)
                ) {
                    Text("Get Range")
                }
            }
            Spacer(modifier = Modifier.height(16.dp))
            Text(
                text = "History",
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.primary
            )
            val scrollState = rememberScrollState()
            LaunchedEffect(history) {
                scrollState.animateScrollTo(scrollState.maxValue)
            }
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(vertical = 8.dp)
                    .verticalScroll(scrollState)
            ) {
                Text(text = history)
            }
            Spacer(modifier = Modifier.height(8.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                OutlinedTextField(
                    value = message,
                    onValueChange = { message = it },
                    label = { Text("Message") },
                    enabled = isConnected,
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Button(
                    onClick = {
                        if (message.isNotBlank()) {
                            onSendMessage(message)
                            message = ""
                        }
                    },
                    enabled = isConnected
                ) {
                    Text("Send")
                }
            }
        }
    }
}

@Preview(showBackground = true)
@Composable
fun MainScreenPreview() {
    OrbweaverTheme {
        MainScreen(
            history = AnnotatedString("Test history"),
            isConnected = false,
            sourceAddress = "254",
            destinationAddress = "255",
            localDevice = "",
            selectedBitRate = "50 kbps",
            selectedChannel = "26",
            onSourceAddressChange = {},
            onDestinationAddressChange = {},
            onBitRateChange = {},
            onChannelChange = {},
            onEchoClick = {},
            onGetRangeClick = {},
            onConnectClick = {},
            onSendMessage = {}
        )
    }
}
