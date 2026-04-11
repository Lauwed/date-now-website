<script>
	let { isLive } = $props();

	const channelLink = 'https://www.twitch.tv/devgirl_';
</script>

{#if isLive}
	<a href={channelLink} target="_blank" class:is-live={isLive}>🎉 Live Now! Join me on Twitch </a>
{/if}

<style lang="scss">
	@use './../styles/mixins.scss' as *;

	a {
		cursor: pointer;
		transition:
			background-color 0.3s,
			color 0.3s;
		position: relative;

		@include lg {
			font-size: 0.8rem;
		}
	}

	a.is-live {
		background-color: #ff4d4d;
		animation: pulse 1.5s infinite;
	}

	@keyframes pulse {
		0% {
			box-shadow: 0 0 0 0 rgba(255, 77, 77, 0.7);
		}
		70% {
			box-shadow: 0 0 0 10px rgba(255, 77, 77, 0);
		}
		100% {
			box-shadow: 0 0 0 0 rgba(255, 77, 77, 0);
		}
	}
</style>
