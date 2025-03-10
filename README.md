# BlackJack Web Services

Una implementación backend del juego de Blackjack utilizando **servicios web basados en SOAP**. Este proyecto expone la lógica del juego a través de un servicio SOAP, permitiendo gestionar partidas de Blackjack (creación, acciones de juego...) mediante mensajes XML.

## Descripción

El objetivo de este proyecto es ofrecer una solución modular y escalable para gestionar el juego de Blackjack a través de servicios web SOAP. La lógica del juego se implementa siguiendo las reglas tradicionales, y se expone a través de un servicio que recibe y responde mensajes SOAP. Esto facilita la integración con aplicaciones cliente que consuman servicios SOAP, como aplicaciones empresariales o sistemas legacy.

## Características

- **Servicios SOAP:** Comunicación basada en mensajes XML estructurados según el protocolo SOAP.
- **Lógica de juego robusta:** Implementación de las reglas clásicas de Blackjack.
- **Modularidad:** Código organizado para facilitar la extensión y el mantenimiento.
- **Escalabilidad:** Diseñado para integrarse en entornos distribuidos y soportar múltiples sesiones simultáneas.

## Requisitos

- **Lenguaje y Framework:** *(C)*
- **Servidor de Aplicaciones:** Un contenedor o servidor compatible con servicios SOAP.
